#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
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

	/* Default sudoku size */
	sudoku_size = 9;

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	printf("[%3.6f] - Rank 0 - Program start\n", 0.0);

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
				return EXIT_FAILURE;
			}
		}

		if (optind < argc) {
			filename = argv[optind];
		} else {
			fprintf(stderr, "Error: Missing filename\n");

			return EXIT_FAILURE;
		}

		/* Basic Validation */
		if (sudoku_size < 1) {
			return EXIT_FAILURE;
		}

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
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
    file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Unable to open file %s\n", filename);
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

    local_threads = n_threads;
    total_threads_allocated = n_threads;
    total_threads_requested = n_threads;

    free(all_thread_counts);

    /* Scan file to compute byte offsets for each rank's start position */
    fclose(file);
	}

    clock_gettime(CLOCK_MONOTONIC, &ts);
    computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
    printf("[%3.6f] - Rank %d - Opening file\n", computation_time, rank);

    file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Rank %d: Error opening file %s\n", rank, filename);
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
	printf("[%3.6f] - Rank 0 - All ready\n", computation_time);

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

    /* Launch Threads */
    t_targets = (target *) malloc(local_threads * sizeof(target));
    // TODO change into triplets
    for (i = 0; i < local_threads; ++i) t_targets[i] = &pthreads_solver;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
    printf("[%3.6f] - Rank 0 - Starting processing\n", computation_time);

    tg = create_thread_group(t_targets, NULL, local_threads);
    join_thread_group(tg, outputs);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    computation_time = (ts.tv_sec - ts_start.tv_sec) + (ts.tv_nsec - ts_start.tv_nsec) / 1e9;
    printf("[%3.6f] - Rank 0 - Completed processing\n", computation_time);

	if (local_output_buffer) free(local_output_buffer);
	return 0;
}
