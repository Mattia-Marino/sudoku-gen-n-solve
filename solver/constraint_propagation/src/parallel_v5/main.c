#define _POSIX_C_SOURCE 199309L

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

typedef void *( *target )( void *);

typedef struct range {
	int start;
	int end;
} range;

/* Worker thread function, solves sudoku puzzles in all_grids in-place */
void *pthreads_solver()
{
	int i;
	range r;

	if (!isEmpty(q_range)) {
		/* Fetch the range */
		dequeue(q_range, &r);
		printf("\tMy range is [%d - %d]\n", r.start, r.end);

		/* Solve the assigned sudoku puzzles in-place */
		for (i = r.start; i <= r.end; ++i)
				sudoku_solver(all_grids[i], sudoku_size);
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

		/* Calculate line distribution across ranks */
		int lines_per_rank = total_lines / size;
		int remainder = total_lines % size;

		all_line_counts = (int *) malloc(size * sizeof(int));
		all_byte_offsets = (long *) malloc(size * sizeof(long));

		for (i = 0; i < size; i++)
			all_line_counts[i] = lines_per_rank + (i < remainder ? 1 : 0);

		/* Scan file to compute byte offsets for each rank's start position */
		all_byte_offsets[0] = 0;
		{
			int ch;
			int total_newlines = 0;
			int next_boundary = all_line_counts[0];
			int rank_idx = 1;

			while (rank_idx < size && (ch = fgetc(file)) != EOF) {
				if (ch == '\n') {
					total_newlines++;
					if (total_newlines == next_boundary) {
						all_byte_offsets[rank_idx] = ftell(file);
						next_boundary += all_line_counts[rank_idx];
						rank_idx++;
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

	MPI_Barrier(MPI_COMM_WORLD);		/* Barrier needed to sync, otherwise rank 0 will start processing before the others*/
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

				r.start = i * load;
				r.end = r.start + load - 1;

				enqueue(q_range, &r);
			}
		}


		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Starting processing\n", computation_time, rank);

		/* Launch Threads */
		t_targets = (target *) malloc(local_threads * sizeof(target));
		for (i = 0; i < local_threads; ++i) t_targets[i] = &pthreads_solver;
		tg = create_thread_group(t_targets, NULL, local_threads);
		join_thread_group(tg, outputs);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Completed processing\n", computation_time, rank);

		/* ******************************************************************
		* PREPARE OUTPUT FOR GATHER
		* ******************************************************************/
		{
			int grid_string_len = get_grid_string_size(sudoku_size);
			int local_output_size_bytes = local_num_lines * grid_string_len;
			char *curr_out_ptr;

			local_output_buffer = (char *) malloc(local_output_size_bytes);
			curr_out_ptr = local_output_buffer;

			for (i = 0; i < local_num_lines; ++i) {
				write_grid_to_string(all_grids[i], curr_out_ptr, sudoku_size);
				curr_out_ptr += grid_string_len;
				free_grid(all_grids[i], sudoku_size);
			}
		}

		free(all_grids);
		free(t_targets);

	} else {
		/* No work for this rank */
		local_output_buffer = (char *) malloc(1);
	}

	/* ******************************************************************
	* GATHER RESULTS
	* ******************************************************************/
	{
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering results\n", computation_time, rank);

		int *recvcounts = NULL;
		int *rdispls = NULL;
		int my_output_bytes;

		if (rank == 0) {
			recvcounts = (int *) malloc(size * sizeof(int));
			rdispls = (int *) malloc(size * sizeof(int));
		}

		/* All ranks calculate how many bytes they generated */
		my_output_bytes = (local_threads > 0 && local_num_lines > 0)
						? (int)(local_num_lines * get_grid_string_size(sudoku_size))
						: 0;

		/* Gather the output sizes to Rank 0 */
		MPI_Gather(&my_output_bytes, 1, MPI_INT,
				   recvcounts, 1, MPI_INT,
				   0, MPI_COMM_WORLD);

		/* Rank 0 calculates displacements for Gatherv */
		if (rank == 0) {
			int current_disp = 0;
			for (i = 0; i < size; i++) {
				rdispls[i] = current_disp;
				current_disp += recvcounts[i];
			}
			final_output_buffer = (char *) malloc(current_disp + 1);
		}

		/* Gather the actual solution strings */
		MPI_Gatherv(local_output_buffer, my_output_bytes, MPI_CHAR,
					final_output_buffer, recvcounts, rdispls, MPI_CHAR,
					0, MPI_COMM_WORLD);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering completed\n", computation_time, rank);

		/* ******************************************************************
		* FINALIZATION (Rank 0)
		* ******************************************************************/
		if (rank == 0) {
			clock_gettime(CLOCK_MONOTONIC, &end_time);
			computation_time = (end_time.tv_sec - start_time.tv_sec) +
							   (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

			printf("\nTotal computation completed in %.6f seconds.\n",
				   computation_time);

			/* Check solutions */
			tot_solved = 0;
			grid_len = get_grid_string_size(sudoku_size);
			current_position = final_output_buffer;
			for (i = 0; i < total_lines; ++i) {
				if (check_solved_string(current_position, sudoku_size))
					++tot_solved;
				current_position += grid_len;
			}
			printf("Total correctly solved sudoku puzzles: %d\n", tot_solved);

			free(all_line_counts);
			free(all_byte_offsets);
			free(recvcounts);
			free(rdispls);
			free(final_output_buffer);
		}
	}

	if (local_output_buffer) free(local_output_buffer);

	MPI_Finalize();
	return 0;
}
