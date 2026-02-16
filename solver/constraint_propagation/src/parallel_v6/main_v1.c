#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <mpi.h>
#include <string.h>

#include "../../include/pthread_groups.h"
#include "../../include/queue.h"
#include "../../include/sudoku_utils.h"
#include "../../include/debug.h"
#include "../../include/solver.h"

void *outputs[15];

int ***all_grids;	/* All the grids assigned to the computational node */
int sudoku_size;	/* The size of the sudoku */
queue *q_range;		/* The queue containing the operating range for each thread */
queue *q_thread_id;

int **common_grid;

typedef void *( *target )( void *);

typedef struct range {
	int id;
	int start;
	int end;
} range;

typedef struct {
	pthread_barrier_t barrier_ready;
	pthread_barrier_t barrier_done;
	volatile int data_is_over;
} tg_sync;

tg_sync *group_sync;

/* Worker thread function, solves sudoku puzzles in all_grids in-place */
void *pthreads_solver()
{
	int i;
	int id;
	range r;

	/* Fetch thread ID */
	dequeue(q_thread_id, &id);
	printf("Thread %d is ready\n", id);

	if (id == 1) {
		/* Provider thread: fetch range and feed puzzles to the group */
		dequeue(q_range, &r);
		printf("\tThreadgroup ID: %d - Our range is [%d - %d]\n", r.id, r.start, r.end);

		for (i = r.start; i <= r.end; ++i) {
			/* Load the next puzzle into the shared grid */
			common_grid = all_grids[i];

			/* Signal all threads that data is ready */
			pthread_barrier_wait(&group_sync->barrier_ready);

			/* Process (display) the sudoku */
			printf("\n\nI am thread %d and I got this sudoku:\n", id);
			display_sudoku(common_grid, sudoku_size);

			/* Wait for all threads to finish processing */
			pthread_barrier_wait(&group_sync->barrier_done);
		}

		/* Signal that there is no more data */
		group_sync->data_is_over = 1;
		pthread_barrier_wait(&group_sync->barrier_ready);
	} else {
		/* Worker threads: wait for data, process, repeat */
		while (1) {
			/* Wait for the provider to load data */
			pthread_barrier_wait(&group_sync->barrier_ready);

			/* Check if all puzzles have been processed */
			if (group_sync->data_is_over)
				break;

			/* Process (display) the sudoku */
			printf("\n\nI am thread %d and I got this sudoku:\n", id);
			display_sudoku(common_grid, sudoku_size);

			/* Signal that processing is done */
			pthread_barrier_wait(&group_sync->barrier_done);
		}
	}

	return NULL;
}

int main(int argc, char **argv)
{
	/* MPI Variables */
	int rank, size;

	/* Work Distribution */
	long *all_byte_offsets = NULL;	/* Rank 0: byte offsets for each rank */
	int *all_line_counts = NULL;	/* Rank 0: number of lines for each rank */
	long my_byte_offset = 0;	/* This rank's starting byte offset */

	/* Data Buffers */
	char *local_output_buffer = NULL;
	char *final_output_buffer = NULL;

	/* Logic Variables */
	int local_num_lines = 0;
	int total_lines = 0;
	int n_threads = 0;
	int total_threads_requested;
	int local_threads;
	const int MAX_THREADS_PER_PROCESS = 15;

	/* Parsing and Processing */
	char *filename = NULL;
	FILE *file;
	int opt, i;
	target *t_targets;
	struct ThreadGroup *tg;

	/* Post-processing */
	int tot_solved;
	int grid_len;
	char *current_position;

	/* Timing */
	struct timespec start_time, end_time;
	double computation_time;

	/* For debugging purposes */
	struct timespec ts;
	struct timespec ts_start;

	/* Initialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Default sudoku size */
	sudoku_size = 9;

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	printf("[%3.6f] - Rank %d - Program start\n", 0.0, rank);

	/* ******************************************************************
	* ALL RANKS: ARGUMENT PARSING
	* All MPI processes parse command-line arguments independently,
	* eliminating the need to broadcast configuration values.
	* ******************************************************************/
	{
		int sqrt_n;

		static struct option long_options[] = {
			{"nthreads", required_argument, 0, 'n'},
			{"size",     required_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
			switch (opt) {
			case 'n': n_threads = atoi(optarg); break;
			case 's': sudoku_size = atoi(optarg); break;
			default:
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}
		}

		if (optind < argc) {
			filename = argv[optind];
		} else {
			if (rank == 0)
				fprintf(stderr, "Error: Missing filename\n");

			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Basic Validation */
		if (sudoku_size < 1) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
	}

	/* ******************************************************************
	* RANK 0: COMPUTE WORK DISTRIBUTION
	* Instead of reading the entire file and scattering the raw content,
	* rank 0 only counts lines and computes byte offsets so that each
	* rank can independently seek and read its own portion.
	* Work is distributed proportionally to thread count per rank.
	* ******************************************************************/
	if (rank == 0) {
		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Start Timing */
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		/* Count total lines (rewinds file internally) */
		total_lines = count_lines_in_file(file);

		DPRINTF("Rank 0: Found %d lines.\n", total_lines);

		/* First, calculate thread distribution across all ranks */
		int *all_thread_counts = (int *) malloc(size * sizeof(int));
		int total_threads_allocated = 0;
		
		for (i = 0; i < size; i++) {
			int threads_before = i * MAX_THREADS_PER_PROCESS;
			if (threads_before >= n_threads) {
				all_thread_counts[i] = 0;
			} else {
				int remaining = n_threads - threads_before;
				all_thread_counts[i] = (remaining > MAX_THREADS_PER_PROCESS)
									 ? MAX_THREADS_PER_PROCESS : remaining;
			}
			total_threads_allocated += all_thread_counts[i];
		}

		/* Calculate line distribution based on thread counts */
		all_line_counts = (int *) malloc(size * sizeof(int));
		all_byte_offsets = (long *) malloc(size * sizeof(long));

		if (total_threads_allocated > 0) {
			int lines_assigned = 0;
			for (i = 0; i < size; i++) {
				if (i == size - 1) {
					/* Last rank gets remaining lines to handle rounding */
					all_line_counts[i] = total_lines - lines_assigned;
				} else {
					/* Proportional distribution based on thread count */
					all_line_counts[i] = (int)((double)all_thread_counts[i] / 
											   total_threads_allocated * total_lines);
					lines_assigned += all_line_counts[i];
				}
			}
		} else {
			/* No threads allocated - should not happen */
			for (i = 0; i < size; i++)
				all_line_counts[i] = 0;
		}
		
		free(all_thread_counts);

		/* Scan file to compute byte offsets for each rank's start position */
		all_byte_offsets[0] = 0;
		{
			int ch;
			int total_newlines = 0;
			int next_boundary = all_line_counts[0];
			int rank_idx = 1;

			/* Handle case where rank 0 has no lines */
			while (rank_idx < size && all_line_counts[rank_idx - 1] == 0) {
				all_byte_offsets[rank_idx] = 0;
				next_boundary += all_line_counts[rank_idx];
				rank_idx++;
			}

			while (rank_idx < size && (ch = fgetc(file)) != EOF) {
				if (ch == '\n') {
					total_newlines++;
					if (total_newlines == next_boundary) {
						all_byte_offsets[rank_idx] = ftell(file);
						next_boundary += all_line_counts[rank_idx];
						rank_idx++;
						/* Skip ranks with no lines */
						while (rank_idx < size && all_line_counts[rank_idx] == 0) {
							all_byte_offsets[rank_idx] = all_byte_offsets[rank_idx - 1];
							rank_idx++;
						}
					}
				}
			}
		}

		fclose(file);
	}

	/* ******************************************************************
	* DATA DISTRIBUTION
	* Instead of scattering the entire file content (MPI_Scatterv),
	* we scatter only the line counts and byte offsets - a few bytes
	* per rank, rather than megabytes of raw puzzle data.
	* ******************************************************************/
	MPI_Scatter(all_line_counts, 1, MPI_INT,
				&local_num_lines, 1, MPI_INT,
				0, MPI_COMM_WORLD);
	MPI_Scatter(all_byte_offsets, 1, MPI_LONG,
				&my_byte_offset, 1, MPI_LONG,
				0, MPI_COMM_WORLD);

	/* Broadcast total_lines (needed by rank 0 for final verification) */
	MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

	DPRINTF("Rank %d: assigned %d lines at byte offset %ld\n",
			rank, local_num_lines, my_byte_offset);

	/* Calculate local threads based on total requested */
	total_threads_requested = n_threads;
	{
		int threads_before = rank * MAX_THREADS_PER_PROCESS;
		if (threads_before >= total_threads_requested) {
			local_threads = 0;
		} else {
			int remaining = total_threads_requested - threads_before;
			local_threads = (remaining > MAX_THREADS_PER_PROCESS)
						  ? MAX_THREADS_PER_PROCESS : remaining;
		}
	}

	/* ******************************************************************
	* LOCAL I/O AND PROCESSING
	* Each rank opens the file independently, seeks to its byte offset,
	* reads only its assigned lines, then solves with pthreads.
	* This distributes the I/O load across all nodes instead of
	* funneling everything through rank 0.
	* ******************************************************************/

	MPI_Barrier(MPI_COMM_WORLD);	/* Barrier needed to sync, otherwise rank 0 will start processing before the others*/
	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - All ready\n", computation_time, rank);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - I have %d threads\n", computation_time, rank, local_threads);

	if (local_threads > 0 && local_num_lines > 0) {
		/* Open file and seek to our starting position */
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Opening file\n", computation_time, rank);

		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Rank %d: Error opening file %s\n", rank, filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
		fseek(file, my_byte_offset, SEEK_SET);

		/* Parse our assigned lines directly into grids */
		all_grids = (int ***) malloc(local_num_lines * sizeof(int **));

		{
			char line_buf[8192];
			for (i = 0; i < local_num_lines; i++) {
				all_grids[i] = create_grid(sudoku_size);
				if (fgets(line_buf, sizeof(line_buf), file) != NULL) {
					/* Remove trailing newline for parser */
					char *nl = strchr(line_buf, '\n');
					if (nl) *nl = '\0';
					if (read_grid_from_string(all_grids[i], line_buf,
											  sudoku_size) != 0) {
						fprintf(stderr,
								"Error parsing grid on rank %d, line %d\n",
								rank, i);
					}
				}
			}
		}
		fclose(file);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Closing file\n", computation_time, rank);

		q_range = createQueue(sizeof(range));
		{
			range r;
			int load;
			int base_load = local_num_lines / local_threads;
			int rem_load = local_num_lines % local_threads;
			for (i = 0; i < local_threads; ++i) {
				load = base_load + (i < rem_load ? 1 : 0);

				r.id = i;
				r.start = i * load;
				r.end = r.start + load - 1;

				enqueue(q_range, &r);
			}
		}

		q_thread_id = createQueue(sizeof(int));
		for (i = 0; i < 3; ++i) {
			int id = i + 1;
			enqueue(q_thread_id, &id);
		}

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Starting processing\n", computation_time, rank);

		/* Initialize thread group synchronization */
		group_sync = (tg_sync *) malloc(sizeof(tg_sync));
		pthread_barrier_init(&group_sync->barrier_ready, NULL, 3);
		pthread_barrier_init(&group_sync->barrier_done, NULL, 3);
		group_sync->data_is_over = 0;

		/* Launch Threads */
		t_targets = (target *) malloc(3 * sizeof(target));
		for (i = 0; i < 3; ++i) t_targets[i] = &pthreads_solver;
		tg = create_thread_group(t_targets, NULL, 3);
		join_thread_group(tg, outputs);

		/* Clean up thread group synchronization */
		pthread_barrier_destroy(&group_sync->barrier_ready);
		pthread_barrier_destroy(&group_sync->barrier_done);
		free(group_sync);

	} else {
		/* No work for this rank */
		local_output_buffer = (char *) malloc(1);
	}

	if (local_output_buffer) free(local_output_buffer);

	MPI_Finalize();
	return 0;
}
