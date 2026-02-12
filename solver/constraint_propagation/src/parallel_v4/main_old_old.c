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


void *outputs[64];

queue *q_n_sudoku;		/* The number of sudoku puzzles assigned to each thread */
queue *q_in;			/* The queue to fill with sudokus to solve */
queue *q_out;			/* The queue to receive the solved sudokus */
int sudoku_size;		/* The size of the sudoku , must be a perfect square (4, 9, 16, ...) */
// int max_sudoku_fetch;


typedef void *( *target )( void *);


void *pthreads_solver()
{
	// int **grid;

	int num_sudoku;
	int ***all_grids;
	size_t i;
	size_t elements;

	if (!isEmpty(q_n_sudoku)) {
		dequeue(q_n_sudoku, &num_sudoku);

		all_grids = (int ***) malloc(num_sudoku * sizeof(int **));
		// all_grids = (int ***) malloc(max_sudoku_fetch * sizeof(int **));

		elements = batchDequeue(q_in, all_grids, num_sudoku);
		// elements = batchDequeue(q_in, all_grids, max_sudoku_fetch);
		for (i = 0; i < elements; ++i)
			sudoku_solver(all_grids[i], sudoku_size);

		batchEnqueue(q_out, all_grids, elements);
		// free(all_grids);
	}

	return NULL;
}


int main(int argc, char **argv)
{
	int ***all_grids;
	int **grid;			/* The representation of the current sudoku */
	int sqrt_n;			/* The square root of n, needed for checks and cached for performance */
	char *filename;			/* The name of the file to read as input */
	FILE *file;			/* The file given in input */
	
	int read_status;

	target *t_targets;
	struct ThreadGroup *tg;

	int opt;
	int n_threads;			/* Number of threads to be activated in a single group */
	int total_threads;		/* Total threads requested across all processes */
	int local_threads;		/* Threads assigned to this MPI process */
	const int MAX_THREADS_PER_PROCESS = 15;

	int num_lines_assigned;

	int i, j, k;
	int tot_solved;

	struct timespec start_time;
	struct timespec end_time;
	double computation_time;

	/* MPI variables */
	int rank, size;
	int total_lines;
	int *thread_counts;
	int *line_distribution;
	int *start_lines;
	int start_line, end_line, num_lines;
	char *line_buffer;
	char **sudoku_lines;
	size_t buffer_size;
	int max_line_length = 10000;
	int line_length;

	int grid_size;
	char *line;
	char **solved_sudoku_puzzles;


	/* Inizialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Default values for n_threads and n */
	n_threads = 0;
	sudoku_size = 9;

	if (rank == 0) {

	/* ********** PREPARATORY PHASE ********** */

		DPRINTF("There are %d MPI processes\n\n", size);

		static struct option long_options[] = {
			{"nthreads", required_argument, 0, 'n'},	/* Long option: --nthreads, short: -n */
			{"size",     required_argument, 0, 's'},	/* Long option: --size, short: -s */
			{0, 0, 0, 0}
		};

		/* "n:s:" means both 'n' and 's' require an argument */
		while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
			switch (opt) {
				case 'n':
					n_threads = atoi(optarg);
					break;
				case 's':
					sudoku_size = atoi(optarg);
					break;
				default:
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
			}
		}

		/* After parsing flags, optind is the index of the next element in argv */
		if (optind < argc) {
			filename = argv[optind];
			printf("File name: %s\n", argv[optind]);
		} else {
			fprintf(stderr, "Error: Missing filename\n");

			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		printf("Given args:\n");
		printf("- N. threads: %d\n", n_threads);
		printf("- Size: %d\n", sudoku_size);
		printf("- File name: %s\n\n", filename);

		/* Checks on puzzle size */
		if (sudoku_size < 1) {
			fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
			
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
			fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", sudoku_size);
			
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Open the file */
		file = fopen(filename, "r");
		if (file == NULL) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);

			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Start timing the computation */
		clock_gettime(CLOCK_MONOTONIC, &start_time);


	/* ********** CONTROLLER PHASE ********** */

		/* Broadcast sudoku_size from rank 0 to all processes */
		MPI_Bcast(&sudoku_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* Broadcast total_threads to all processes */
		total_threads = n_threads;
		MPI_Bcast(&total_threads, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* Calculate local threads for this process */
		int threads_before = rank * MAX_THREADS_PER_PROCESS;
		if (threads_before >= total_threads) {
			local_threads = 0;
		} else {
			int remaining = total_threads - threads_before;
			local_threads = (remaining > MAX_THREADS_PER_PROCESS) ? MAX_THREADS_PER_PROCESS : remaining;
		}

		DPRINTF("I am rank %d, sudoku size is: %d, assigned %d threads\n", 
		       rank, sudoku_size, local_threads);

		/* Count total lines in the file */
		total_lines = 0;
		while (fgets(line_buffer = malloc(max_line_length), max_line_length, file) != NULL) {
			total_lines++;
			free(line_buffer);
		}
		rewind(file);

		DPRINTF("Total lines in file: %d\n\n", total_lines);

		/* Calculate thread distribution for all ranks */
		thread_counts = (int *)malloc(size * sizeof(int));
		line_distribution = (int *)malloc(size * sizeof(int));
		start_lines = (int *)malloc(size * sizeof(int));

		for (i = 0; i < size; i++) {
			int threads_before = i * MAX_THREADS_PER_PROCESS;
			if (threads_before >= total_threads) {
				thread_counts[i] = 0;
			} else {
				int remaining = total_threads - threads_before;
				thread_counts[i] = (remaining > MAX_THREADS_PER_PROCESS) ? MAX_THREADS_PER_PROCESS : remaining;
			}
		}

		/* Calculate line distribution proportional to thread count */
		int assigned_lines = 0;
		for (i = 0; i < size; i++) {
			if (i == size - 1) {
				/* Last rank gets remaining lines to avoid rounding issues */
				line_distribution[i] = total_lines - assigned_lines;
			} else {
				line_distribution[i] = (total_lines * thread_counts[i]) / total_threads;
				assigned_lines += line_distribution[i];
			}
			start_lines[i] = (i == 0) ? 0 : start_lines[i-1] + line_distribution[i-1];
		}

		/* Print distribution plan */
		DPRINTF("Line distribution plan:\n");
		for (i = 0; i < size; i++) {
			if (thread_counts[i] > 0) {
				DPRINTF("  Rank %d: lines %d-%d (%d lines, %d threads, %.1f%%)\n", 
				       i, start_lines[i], start_lines[i] + line_distribution[i] - 1,
				       line_distribution[i], thread_counts[i],
				       100.0 * line_distribution[i] / total_lines);
			}
		}
		DPRINTF("\n");

		/* Send distribution info and actual file data to all ranks */
		rewind(file);
		int current_line = 0;
		line_buffer = (char *)malloc(max_line_length * sizeof(char));

		/* Allocate storage for rank 0's sudoku lines */
		sudoku_lines = (char **)malloc(line_distribution[0] * sizeof(char *));
		for (i = 0; i < line_distribution[0]; i++) {
			sudoku_lines[i] = (char *)malloc(max_line_length * sizeof(char));
		}

		/* Read file and distribute to ranks */
		int rank0_count = 0;
		int current_rank = 0;
		int rank_line_count = 0;

		while (fgets(line_buffer, max_line_length, file) != NULL) {
			/* Determine which rank this line belongs to */
			while (current_rank < size && current_line >= start_lines[current_rank] + line_distribution[current_rank]) {
				current_rank++;
			}

			if (current_rank >= size) break;

			if (current_rank == 0) {
				/* Store for rank 0 */
				strcpy(sudoku_lines[rank0_count], line_buffer);
				rank0_count++;
			} else if (thread_counts[current_rank] > 0) {
				/* Send to other rank */
				if (current_line == start_lines[current_rank]) {
					/* First line for this rank - send count first */
					MPI_Send(&line_distribution[current_rank], 1, MPI_INT, current_rank, 0, MPI_COMM_WORLD);
				}
				/* Send the actual line */
				line_length = strlen(line_buffer) + 1;
				MPI_Send(&line_length, 1, MPI_INT, current_rank, 2, MPI_COMM_WORLD);
				MPI_Send(line_buffer, line_length, MPI_CHAR, current_rank, 3, MPI_COMM_WORLD);
			}

			current_line++;
		}

		free(line_buffer);
		free(thread_counts);
		free(line_distribution);
		free(start_lines);

		/* Store rank 0's assignment */
		num_lines = rank0_count;

		fclose(file);
	} else {
		/* Broadcast sudoku_size from rank 0 to all processes */
		MPI_Bcast(&sudoku_size, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* Broadcast total_threads to all processes */
		MPI_Bcast(&total_threads, 1, MPI_INT, 0, MPI_COMM_WORLD);

		/* Calculate local threads for this process */
		int threads_before = rank * MAX_THREADS_PER_PROCESS;
		if (threads_before >= total_threads) {
			local_threads = 0;
		} else {
			int remaining = total_threads - threads_before;
			local_threads = (remaining > MAX_THREADS_PER_PROCESS) ? MAX_THREADS_PER_PROCESS : remaining;
		}

		DPRINTF("I am rank %d, sudoku size is: %d, assigned %d threads\n", 
		       rank, sudoku_size, local_threads);

		/* Receive distribution info and actual file data if this rank has threads */
		if (local_threads > 0) {
			MPI_Recv(&num_lines, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

			/* Allocate storage for sudoku lines */
			sudoku_lines = (char **)malloc(num_lines * sizeof(char *));

			/* Receive each sudoku line */
			for (i = 0; i < num_lines; i++) {
				MPI_Recv(&line_length, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				sudoku_lines[i] = (char *)malloc(line_length * sizeof(char));
				MPI_Recv(sudoku_lines[i], line_length, MPI_CHAR, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
		}
	}

	if (local_threads > 0) {
		all_grids = (int ***) malloc(num_lines * sizeof(int **));

		q_in = createQueue(sizeof(grid));
		q_out = createQueue(sizeof(grid));

		DPRINTF("\nRank %d: Received %d sudoku puzzles\n", rank, num_lines);
		for (i = 0; i < num_lines; ++i){
			DPRINTF("%s", sudoku_lines[i]);

			/* Allocate memory for the Sudoku grid */
			all_grids[i] = create_grid(sudoku_size);
			if (all_grids[i] == NULL) {
				fprintf(stderr, "Error: Failed to allocate memory for grid\n");
				fclose(file);

				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
			}

			/* Read the Sudoku grid from the file */
			read_status = read_grid_from_string(all_grids[i], sudoku_lines[i], sudoku_size);

			if (read_status != 0) {
				/* If read failed because we reached EOF */
				if (feof(file)) {
					DPRINTF("Reached EOF\n");
					break;
				} else {
					fprintf(stderr, "Error: Failed to read grid from file\n");
		
					free_grid(all_grids[i], sudoku_size);
					
					MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
					return EXIT_FAILURE;
				}
			}
		}

		/* Enqueue all the sudoku puzzles */
		batchEnqueue(q_in, all_grids, num_lines);

		for (i = 0; i < num_lines; i++)
			free(sudoku_lines[i]);

		free(sudoku_lines);

		/* Compute number of lines per thread */
		q_n_sudoku = createQueue(sizeof(int));
		for (i = 0; i < local_threads - 1; ++i) {
			num_lines_assigned = num_lines/local_threads;
			enqueue(q_n_sudoku, &num_lines_assigned);
		}

		num_lines_assigned = (num_lines/local_threads) + (num_lines%local_threads);
		enqueue(q_n_sudoku, &num_lines_assigned);
		// max_sudoku_fetch = (int) ceil((double) num_lines / (double) local_threads);

		/* Start threadgroup */
		t_targets = (target *) malloc(local_threads * sizeof(target));
		for (i = 0; i < local_threads; ++i)
			t_targets[i] = &pthreads_solver;

		tg = create_thread_group(t_targets, NULL, local_threads);
		join_thread_group(tg, outputs);

		grid_size = get_grid_string_size(sudoku_size);

		if (rank == 0) {
			solved_sudoku_puzzles = (char **) malloc(total_lines * sizeof(char *));
			for (i = 0; i < total_lines; ++i)
				solved_sudoku_puzzles[i] = (char *) malloc(grid_size * sizeof(char));

			/* Process rank 0's own results */
			i = 0;
			while (!isEmpty(q_out)) {
				dequeue(q_out, &grid);

				write_grid_to_string(grid, solved_sudoku_puzzles[i], sudoku_size);

				free_grid(grid, sudoku_size);

				++i;
			}

			/* Receive results from other ranks */
			if (size > 1) {
				for (j = 1; j < size; ++j) {
					int recv_count;
					MPI_Recv(&recv_count, 1, MPI_INT, j, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
					
					for (k = 0; k < recv_count; ++k) {
						MPI_Recv(solved_sudoku_puzzles[i], grid_size, MPI_CHAR, j, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

						++i;
					}
				}
			}
		} else {
			/* Send num_lines first */
			MPI_Send(&num_lines, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

			/* Send all solved sudokus to rank 0 */
			i = 0;
			while (!isEmpty(q_out) && i < num_lines) {
				dequeue(q_out, &grid);

				line = (char *) malloc(grid_size * sizeof(char));

				write_grid_to_string(grid, line, sudoku_size);
				MPI_Send(line, grid_size, MPI_CHAR, 0, 1, MPI_COMM_WORLD);

				free(line);
				free_grid(grid, sudoku_size);

				++i;
			}
		}
	} else {
		DPRINTF("\nRank %d: No work assigned (0 threads)\n", rank);
	}

	if (rank == 0) {

		/* End timing */
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		computation_time = (end_time.tv_sec - start_time.tv_sec) +
				(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
		printf("\nTotal computation completed in %.6f seconds.\n",
		computation_time);

		tot_solved = 0;
		for (i = 0; i < total_lines; ++i)
			if (check_solved_string(solved_sudoku_puzzles[i], sudoku_size))
				++tot_solved;

		printf("\nTotal sudoku puzzles solved: %d\n", tot_solved);

		for (i = 0; i < total_lines; ++i)
			free(solved_sudoku_puzzles[i]);

		free(solved_sudoku_puzzles);
	}

	MPI_Finalize();
	return 0;
}