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

queue *q_n_sudoku;			/* The number of sudoku puzzles assigned to each thread */
queue *q_in;				/* The queue to fill with sudokus to solve */
queue *q_out;				/* The queue to receive the solved sudokus */
int sudoku_size;			/* The size of the sudoku */

typedef void *( *target )( void *);

/* Worker thread function: Consumes from q_in, solves, produces to q_out */
void *pthreads_solver()
{
	int num_sudoku;
	int ***all_grids;
	size_t i;
	size_t elements;

	if (!isEmpty(q_n_sudoku)) {
		dequeue(q_n_sudoku, &num_sudoku);

		all_grids = (int ***) malloc(num_sudoku * sizeof(int **));
		
		/* Fetch a batch of puzzles */
		elements = batchDequeue(q_in, all_grids, num_sudoku);

		/* Solve them */
		for (i = 0; i < elements; ++i)
				sudoku_solver(all_grids[i], sudoku_size);

		/* Push solved puzzles to output queue */
		batchEnqueue(q_out, all_grids, elements);
	}	   
	return NULL;
}

int main(int argc, char **argv)
{
	/* MPI Variables */
	int rank, size;
	int *sendcounts = NULL; /* Bytes to send to each rank */
	int *displs = NULL;	/* Displacement in file buffer for each rank */
	
	/* Data Buffers */
	char *full_file_buffer = NULL;	  /* Rank 0: holds whole file */
	char *local_buffer = NULL;	  /* All ranks: holds local chunk of puzzles */
	char *local_output_buffer = NULL; /* All ranks: holds local solved strings */
	char *final_output_buffer = NULL; /* Rank 0: holds all solutions */

	/* Logic Variables */
	int local_bytes_recvd = 0;
	int local_num_lines = 0;
	int total_lines = 0;
	int n_threads = 0;
	int total_threads_requested;
	int local_threads;
	const int MAX_THREADS_PER_PROCESS = 15; /* 15 processors per node (1 is already MPI) */
	
	/* Parsing and Processing */
	int ***all_grids;
	int **grid;
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

	/* Initialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Default sudoku size */
	sudoku_size = 9;

	/* ******************************************************************
	* RANK 0: ARGUMENT PARSING AND INPUT READING
	* ******************************************************************/
	if (rank == 0) {
		static struct option long_options[] = {
			{"nthreads", required_argument, 0, 'n'},
			{"size",	 required_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		while ((opt = getopt_long(argc, argv, "n:s:", long_options, NULL)) != -1) {
			switch (opt) {
				case 'n': n_threads = atoi(optarg); break;
				case 's': sudoku_size = atoi(optarg); break;
				default: MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); return EXIT_FAILURE;
			}
		}

		if (optind < argc) {
			filename = argv[optind];
		} else {
				fprintf(stderr, "Error: Missing filename\n");
				MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
				return EXIT_FAILURE;
		}

		/* Basic Validation */
		if (sudoku_size < 1) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
			return EXIT_FAILURE; 
		}
		int sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE); 
			return EXIT_FAILURE; 
		}

		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Start Timing */
		clock_gettime(CLOCK_MONOTONIC, &start_time);

		/* Read entire file into memory */
		fseek(file, 0, SEEK_END);
		long filesize = ftell(file);
		fseek(file, 0, SEEK_SET);

		full_file_buffer = (char*) malloc(filesize + 1);
		if(!full_file_buffer) {
			fprintf(stderr, "Memory allocation error\n");
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
		}
		fread(full_file_buffer, 1, filesize, file);
		full_file_buffer[filesize] = '\0';
		fclose(file);

		/* Count lines to calculate distribution */
		total_lines = 0;
		for(long k=0; k<filesize; k++) {
			if(full_file_buffer[k] == '\n') total_lines++;
		}
		/* Handle case where last line has no newline */
		if(filesize > 0 && full_file_buffer[filesize-1] != '\n') total_lines++;

		DPRINTF("Rank 0: Read %ld bytes, %d lines.\n", filesize, total_lines);

		/* Calculate distribution (Scatterv parameters) */
		sendcounts = (int*) malloc(size * sizeof(int));
		displs = (int*) malloc(size * sizeof(int));
		
		/* Calculate how many lines each rank gets */
		int lines_per_rank = total_lines / size;
		int remainder = total_lines % size;
		
		char *curr_ptr = full_file_buffer;
		int accumulated_offset = 0;

		for (i = 0; i < size; i++) {
			displs[i] = accumulated_offset;
			int target_lines = lines_per_rank + (i < remainder ? 1 : 0);
			
			/* Scan forward to find the byte offset after 'target_lines' lines */
			int lines_found = 0;
			int bytes_count = 0;
			while(lines_found < target_lines && *curr_ptr != '\0') {
				if(*curr_ptr == '\n') lines_found++;
				curr_ptr++;
				bytes_count++;
			}
			sendcounts[i] = bytes_count;
			accumulated_offset += bytes_count;
		}
	}

	/* ******************************************************************
	* DATA DISTRIBUTION (SCATTER)
	* ******************************************************************/
	
	/* 1. Broadcast Global Configuration */
	MPI_Bcast(&sudoku_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&n_threads, 1, MPI_INT, 0, MPI_COMM_WORLD);

	/* 2. Distribute Input Data Size to each rank */
	/* Rank 0 already knows sendcounts, we scatter that so each rank knows how much to receive */
	MPI_Scatter(sendcounts, 1, MPI_INT, &local_bytes_recvd, 1, MPI_INT, 0, MPI_COMM_WORLD);

	/* 3. Allocate local buffer and Scatter actual data */
	local_buffer = (char*) malloc((local_bytes_recvd + 1) * sizeof(char));
	MPI_Scatterv(full_file_buffer, sendcounts, displs, MPI_CHAR, 
					local_buffer, local_bytes_recvd, MPI_CHAR, 
					0, MPI_COMM_WORLD);
	
	/* Null terminate local buffer for string processing */
	local_buffer[local_bytes_recvd] = '\0';

	/* Count local lines to confirm workload */
	local_num_lines = 0;
	for(int k=0; k<local_bytes_recvd; k++) {
		if(local_buffer[k] == '\n') local_num_lines++;
	}
	if(local_bytes_recvd > 0 && local_buffer[local_bytes_recvd-1] != '\n') local_num_lines++;

	/* Calculate local threads based on total requested */
	total_threads_requested = n_threads;
	int threads_before = rank * MAX_THREADS_PER_PROCESS;
	if (threads_before >= total_threads_requested) {
		local_threads = 0;
	} else {
		int remaining = total_threads_requested - threads_before;
		local_threads = (remaining > MAX_THREADS_PER_PROCESS) ? MAX_THREADS_PER_PROCESS : remaining;
	}

	/* ******************************************************************
	* LOCAL PROCESSING (THREADS)
	* ******************************************************************/
	
	if (local_threads > 0 && local_num_lines > 0) {
		/* Prepare queues and grids */
		all_grids = (int ***) malloc(local_num_lines * sizeof(int **));
		q_in = createQueue(sizeof(grid));
		q_out = createQueue(sizeof(grid));

		/* Parse local buffer into grids */
		char *line_ptr = strtok(local_buffer, "\n");
		i = 0;
		while (line_ptr != NULL && i < local_num_lines) {
			all_grids[i] = create_grid(sudoku_size);
			/* Note: read_grid_from_string is assumed to be thread-safe or strictly local */
			if(read_grid_from_string(all_grids[i], line_ptr, sudoku_size) != 0) {
					fprintf(stderr, "Error parsing grid on rank %d\n", rank);
			}
			line_ptr = strtok(NULL, "\n");
			i++;
		}

		/* Enqueue all work */
		batchEnqueue(q_in, all_grids, local_num_lines);

		/* Load Balance: Assign puzzles to threads */
		q_n_sudoku = createQueue(sizeof(int));
		int base_load = local_num_lines / local_threads;
		int rem_load = local_num_lines % local_threads;
		for (i = 0; i < local_threads; ++i) {
			int load = base_load + (i < rem_load ? 1 : 0);
			enqueue(q_n_sudoku, &load);
		}

		/* Launch Threads */
		t_targets = (target *) malloc(local_threads * sizeof(target));
		for (i = 0; i < local_threads; ++i) t_targets[i] = &pthreads_solver;
		tg = create_thread_group(t_targets, NULL, local_threads);
		join_thread_group(tg, outputs);

		/* ******************************************************************
		* PREPARE OUTPUT FOR GATHER
		* ******************************************************************/
		
		/* Calculate size of one solved string */
		int grid_string_len = get_grid_string_size(sudoku_size); 
		
		/* Allocate local output buffer */		
		int local_output_size_bytes = local_num_lines * grid_string_len;
		local_output_buffer = (char*) malloc(local_output_size_bytes);
		
		char *curr_out_ptr = local_output_buffer;
		
		while (!isEmpty(q_out)) {
			dequeue(q_out, &grid);
			/* Write grid to string directly into buffer */
			write_grid_to_string(grid, curr_out_ptr, sudoku_size);
				
			/* Move pointer */
			curr_out_ptr += grid_string_len;
			free_grid(grid, sudoku_size);
		}
		
		free(all_grids); // Array of pointers only
		free(t_targets);

	} else {
		/* No work for this rank */
		local_output_buffer = malloc(1); /* Safe malloc for 0 size */
	}

	/* ******************************************************************
	* GATHER RESULTS
	* ******************************************************************/
	
	/* 1. Rank 0 needs to know how many bytes to receive from each rank (recvcounts) */
	int *recvcounts = NULL;
	int *rdispls = NULL;
	
	if (rank == 0) {
		recvcounts = (int*) malloc(size * sizeof(int));
		rdispls = (int*) malloc(size * sizeof(int));
	}

	/* All ranks calculate how many bytes they generated */
	int my_output_bytes = (local_threads > 0) ? (local_num_lines * get_grid_string_size(sudoku_size)) : 0;
	
	/* Gather the output sizes to Rank 0 */
	MPI_Gather(&my_output_bytes, 1, MPI_INT, 
				recvcounts, 1, MPI_INT, 
				0, MPI_COMM_WORLD);

	/* 2. Rank 0 calculates displacements for Gatherv */
	if (rank == 0) {
		int current_disp = 0;
		for(i=0; i<size; i++) {
			rdispls[i] = current_disp;
			current_disp += recvcounts[i];
		}
		/* Allocate final buffer */
		final_output_buffer = (char*) malloc(current_disp + 1); // +1 for safety
	}

	/* 3. Gather the actual solution strings */
	MPI_Gatherv(local_output_buffer, my_output_bytes, MPI_CHAR,
				final_output_buffer, recvcounts, rdispls, MPI_CHAR,
				0, MPI_COMM_WORLD);


	/* ******************************************************************
	* FINALIZATION (Rank 0)
	* ******************************************************************/
	if (rank == 0) {
		clock_gettime(CLOCK_MONOTONIC, &end_time);
		computation_time = (end_time.tv_sec - start_time.tv_sec) +
							(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
		
		printf("\nTotal computation completed in %.6f seconds.\n", computation_time);

		/* Check solutions by iterating through final_output_buffer */
		tot_solved = 0;
		grid_len = get_grid_string_size(sudoku_size);
		current_position = final_output_buffer;
		for (i = 0; i < total_lines; ++i) {
			if (check_solved_string(current_position, sudoku_size))
				++tot_solved;
			current_position += grid_len;
		}
		printf("Total correctly solved sudoku puzzles: %d\n", tot_solved);
		
		free(full_file_buffer);
		free(sendcounts);
		free(displs);
		free(recvcounts);
		free(rdispls);
		free(final_output_buffer);
	}

	if(local_buffer) free(local_buffer);
	if(local_output_buffer) free(local_output_buffer);

	MPI_Finalize();
	return 0;
}