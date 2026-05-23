/* SPDX-License-Identifier: GPL-3.0 */
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
#include "../../include/sudoku_utils.h"
#include "../../include/debug.h"
#include "../../include/solver_parallel_v5.h"

/*
 * parallel_v5: cooperative process-parallelism solver.
 *
 *  - 3-thread groups (strictly), each group cooperatively solves
 *    ONE puzzle at a time (v4-style: row/col/box partition, three
 *    threads share the board). Groups are persistent for the
 *    whole run and reuse a single board across all their puzzles.
 *  - The cooperative solver uses a sense-reversing spin barrier
 *    and atomic ops (no mutexes). With ~100 ns spin-barrier waits
 *    instead of multi-microsecond pthread_barrier waits, the
 *    per-iteration sync overhead drops to a level where the 3-way
 *    parallelism actually beats the serial solver even on the
 *    very easy 9x9 puzzles in the easy_* datasets.
 *  - Ranks are filled "node-first" up to MAX_THREADS_PER_PROCESS,
 *    and threads are grouped strictly in 3s: any leftover threads
 *    that cannot fill a complete group are unused. This enforces
 *    "fill a group of 3 before opening a new threadgroup".
 */

#define MAX_THREADS_PER_PROCESS	15
#define GROUP_SIZE		3

int ***all_grids;	/* All the grids assigned to this MPI rank */
int sudoku_size;	/* The size of the sudoku */

typedef void *( *target )( void *);

/*
 * Per-threadgroup state. One spin barrier (used both for "next
 * puzzle ready" signaling and for the in-solver step synchronization)
 * plus a persistent board reused across the whole puzzle batch.
 */
typedef struct {
	struct spin_barrier barrier;
	struct board *board;
	volatile int data_is_over;
} tg_state;

tg_state *group_states;

typedef struct {
	int tg_id;
	int thread_id;		/* 1-based within the group */
	int tg_size;
	int start;		/* puzzle range, inclusive */
	int end;
} thread_arg;

static volatile int rank_solved;
static pthread_mutex_t solved_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Each group's thread 1 acts as the producer/coordinator: it
 * loads the next puzzle into the persistent board, then enters
 * the spin barrier to release the workers. All three threads then
 * cooperate on the puzzle via solve_board_thread (which itself
 * uses the same spin barrier 4 times per convergence iteration).
 * After the solve returns, thread 1 collects the result and
 * starts the next puzzle while workers wait at the next barrier.
 *
 * On exit, thread 1 sets data_is_over=1 and does one final
 * barrier wait to release workers, which then break the loop.
 */
static void *group_worker(void *arg)
{
	thread_arg *ta = (thread_arg *)arg;
	tg_state *st = &group_states[ta->tg_id];
	int my_id = ta->thread_id;
	int tg_size = ta->tg_size;
	int local_sense = 0;
	int local_solved = 0;
	int i;

	if (my_id == 1) {
		printf("\tThreadgroup %d - Range is [%d - %d] (size %d)\n",
		       ta->tg_id, ta->start, ta->end, tg_size);

		for (i = ta->start; i <= ta->end; ++i) {
			reset_and_populate(st->board, all_grids[i]);

			/* Release workers on this puzzle. */
			spin_barrier_wait(&st->barrier, &local_sense);

			solve_board_thread(st->board, my_id, tg_size,
					   &local_sense);

			/*
			 * solve_board_thread exits collectively (extra
			 * exit-barrier at the end), so the board state
			 * is consistent here and workers are spinning
			 * at the next "puzzle ready" barrier_wait.
			 */
			board_to_grid(st->board, all_grids[i]);
			if (__atomic_load_n(&st->board->unset_cells,
					    __ATOMIC_RELAXED) == 0)
				++local_solved;
		}

		/* Tell workers to break out. */
		st->data_is_over = 1;
		spin_barrier_wait(&st->barrier, &local_sense);

		pthread_mutex_lock(&solved_lock);
		rank_solved += local_solved;
		pthread_mutex_unlock(&solved_lock);
	} else {
		while (1) {
			spin_barrier_wait(&st->barrier, &local_sense);

			if (st->data_is_over)
				break;

			solve_board_thread(st->board, my_id, tg_size,
					   &local_sense);
		}
	}

	free(ta);
	return NULL;
}

int main(int argc, char **argv)
{
	int rank, size;

	long *all_byte_offsets = NULL;
	int *all_line_counts = NULL;
	long my_byte_offset = 0;

	char *local_output_buffer = NULL;
	char *final_output_buffer = NULL;

	int local_num_lines = 0;
	int total_lines = 0;
	int n_threads = 0;
	int total_threads_requested;
	int local_threads;
	int usable_local_threads;

	char *filename = NULL;
	FILE *file;
	int opt, i, g, j;
	int n_threadgroups;
	target *t_targets;
	struct ThreadGroup **thread_groups;
	void *outputs[MAX_THREADS_PER_PROCESS];

	int tot_solved;
	int grid_len;
	char *current_position;

	struct timespec start_time, end_time;
	double computation_time;
	struct timespec ts;
	struct timespec ts_start;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	sudoku_size = 9;

	clock_gettime(CLOCK_MONOTONIC, &ts_start);
	printf("[%3.6f] - Rank %d - Program start\n", 0.0, rank);

	/* ------------------------------------------------------------------
	 * Argument parsing
	 * ------------------------------------------------------------------ */
	{
		int sqrt_n;
		static struct option long_options[] = {
			{ "nthreads", required_argument, 0, 'n' },
			{ "size",     required_argument, 0, 's' },
			{ 0, 0, 0, 0 }
		};

		while ((opt = getopt_long(argc, argv, "n:s:",
					  long_options, NULL)) != -1) {
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

		if (sudoku_size < 1) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size) {
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		if (sudoku_size != 9) {
			if (rank == 0)
				fprintf(stderr,
					"Error: parallel_v5 only supports 9x9 sudokus\n");
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		if (n_threads <= 0) {
			if (rank == 0)
				fprintf(stderr,
					"Error: --nthreads must be > 0\n");
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
	}

	/* ------------------------------------------------------------------
	 * RANK 0: work distribution. Threads are filled rank-first up to
	 * MAX_THREADS_PER_PROCESS, and each rank's load is proportional
	 * to the number of *usable* threads it has (rounded down to a
	 * multiple of GROUP_SIZE because incomplete groups are not opened).
	 * ------------------------------------------------------------------ */
	if (rank == 0) {
		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		clock_gettime(CLOCK_MONOTONIC, &start_time);

		total_lines = count_lines_in_file(file);
		DPRINTF("Rank 0: Found %d lines.\n", total_lines);

		int *all_thread_counts = (int *)malloc(size * sizeof(int));
		int total_threads_allocated = 0;

		for (i = 0; i < size; i++) {
			int threads_before = i * MAX_THREADS_PER_PROCESS;
			int raw;
			if (threads_before >= n_threads) {
				raw = 0;
			} else {
				int remaining = n_threads - threads_before;
				raw = (remaining > MAX_THREADS_PER_PROCESS)
					? MAX_THREADS_PER_PROCESS
					: remaining;
			}
			/* Strip leftover threads that cannot fill a group. */
			all_thread_counts[i] = (raw / GROUP_SIZE) * GROUP_SIZE;
			total_threads_allocated += all_thread_counts[i];
		}

		all_line_counts = (int *)malloc(size * sizeof(int));
		all_byte_offsets = (long *)malloc(size * sizeof(long));

		if (total_threads_allocated > 0) {
			int lines_assigned = 0;
			int last_with_work = -1;
			for (i = 0; i < size; i++)
				if (all_thread_counts[i] > 0)
					last_with_work = i;

			for (i = 0; i < size; i++) {
				if (all_thread_counts[i] == 0) {
					all_line_counts[i] = 0;
					continue;
				}
				if (i == last_with_work) {
					all_line_counts[i] =
						total_lines - lines_assigned;
				} else {
					all_line_counts[i] =
						(int)((double)all_thread_counts[i] /
						      total_threads_allocated *
						      total_lines);
					lines_assigned += all_line_counts[i];
				}
			}
		} else {
			for (i = 0; i < size; i++)
				all_line_counts[i] = 0;
		}

		free(all_thread_counts);

		all_byte_offsets[0] = 0;
		{
			int ch;
			int total_newlines = 0;
			int next_boundary = all_line_counts[0];
			int rank_idx = 1;

			while (rank_idx < size &&
			       all_line_counts[rank_idx - 1] == 0) {
				all_byte_offsets[rank_idx] = 0;
				next_boundary += all_line_counts[rank_idx];
				rank_idx++;
			}

			while (rank_idx < size && (ch = fgetc(file)) != EOF) {
				if (ch == '\n') {
					total_newlines++;
					if (total_newlines == next_boundary) {
						all_byte_offsets[rank_idx] = ftell(file);
						next_boundary +=
							all_line_counts[rank_idx];
						rank_idx++;
						while (rank_idx < size &&
						       all_line_counts[rank_idx] == 0) {
							all_byte_offsets[rank_idx] =
								all_byte_offsets[rank_idx - 1];
							rank_idx++;
						}
					}
				}
			}
		}

		fclose(file);
	}

	MPI_Scatter(all_line_counts, 1, MPI_INT,
		    &local_num_lines, 1, MPI_INT,
		    0, MPI_COMM_WORLD);
	MPI_Scatter(all_byte_offsets, 1, MPI_LONG,
		    &my_byte_offset, 1, MPI_LONG,
		    0, MPI_COMM_WORLD);
	MPI_Bcast(&total_lines, 1, MPI_INT, 0, MPI_COMM_WORLD);

	DPRINTF("Rank %d: assigned %d lines at byte offset %ld\n",
		rank, local_num_lines, my_byte_offset);

	total_threads_requested = n_threads;
	{
		int threads_before = rank * MAX_THREADS_PER_PROCESS;
		if (threads_before >= total_threads_requested) {
			local_threads = 0;
		} else {
			int remaining = total_threads_requested - threads_before;
			local_threads = (remaining > MAX_THREADS_PER_PROCESS)
				? MAX_THREADS_PER_PROCESS
				: remaining;
		}
	}

	/* Drop leftover threads that cannot fill a full group of 3. */
	usable_local_threads = (local_threads / GROUP_SIZE) * GROUP_SIZE;
	if (rank == 0 && usable_local_threads != local_threads) {
		fprintf(stderr,
			"Warning: rank %d has %d threads but only %d are usable "
			"(complete groups of %d only)\n",
			rank, local_threads, usable_local_threads, GROUP_SIZE);
	}

	MPI_Barrier(MPI_COMM_WORLD);
	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) +
		(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - All ready\n", computation_time, rank);

	clock_gettime(CLOCK_MONOTONIC, &ts);
	computation_time = (ts.tv_sec - ts_start.tv_sec) +
		(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
	printf("[%3.6f] - Rank %d - I have %d threads (%d usable)\n",
	       computation_time, rank, local_threads, usable_local_threads);

	if (usable_local_threads > 0 && local_num_lines > 0) {
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Opening file\n",
		       computation_time, rank);

		file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr,
				"Rank %d: Error opening file %s\n",
				rank, filename);
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}
		fseek(file, my_byte_offset, SEEK_SET);

		all_grids = (int ***)malloc(local_num_lines * sizeof(int **));

		{
			char line_buf[8192];
			for (i = 0; i < local_num_lines; i++) {
				all_grids[i] = create_grid(sudoku_size);
				if (fgets(line_buf, sizeof(line_buf), file)
				    != NULL) {
					char *nl = strchr(line_buf, '\n');
					if (nl) *nl = '\0';
					if (read_grid_from_string(all_grids[i],
								  line_buf,
								  sudoku_size)
					    != 0) {
						fprintf(stderr,
							"Error parsing grid on rank %d, line %d\n",
							rank, i);
					}
				}
			}
		}
		fclose(file);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Closing file\n",
		       computation_time, rank);

		/* --------------------------------------------------------
		 * Persistent threadgroup setup. Strict groups of GROUP_SIZE
		 * threads. Each group gets its own state (spin barrier +
		 * reused board) and a contiguous puzzle range.
		 * -------------------------------------------------------- */
		n_threadgroups = usable_local_threads / GROUP_SIZE;

		group_states = (tg_state *)malloc(n_threadgroups *
						  sizeof(tg_state));
		thread_groups = (struct ThreadGroup **)
			malloc(n_threadgroups * sizeof(struct ThreadGroup *));
		rank_solved = 0;

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Starting processing (%d threadgroups)\n",
		       computation_time, rank, n_threadgroups);

		{
			int base_load = local_num_lines / n_threadgroups;
			int rem_load = local_num_lines % n_threadgroups;
			int puzzle_offset = 0;

			for (g = 0; g < n_threadgroups; g++) {
				int load = base_load + (g < rem_load ? 1 : 0);

				spin_barrier_init(&group_states[g].barrier,
						  GROUP_SIZE);
				group_states[g].board = init_board(
					&group_states[g].barrier);
				group_states[g].data_is_over = 0;

				t_targets = (target *)malloc(GROUP_SIZE *
							     sizeof(target));
				void **args = (void **)malloc(GROUP_SIZE *
							      sizeof(void *));

				for (j = 0; j < GROUP_SIZE; j++) {
					thread_arg *ta =
						(thread_arg *)malloc(sizeof(thread_arg));
					ta->tg_id = g;
					ta->thread_id = j + 1;
					ta->tg_size = GROUP_SIZE;
					ta->start = puzzle_offset;
					ta->end = puzzle_offset + load - 1;

					t_targets[j] = &group_worker;
					args[j] = ta;
				}

				thread_groups[g] = create_thread_group(
					t_targets, 0, args, GROUP_SIZE);
				free(t_targets);
				free(args);

				puzzle_offset += load;
			}
		}

		for (g = 0; g < n_threadgroups; g++)
			join_thread_group(thread_groups[g], outputs);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Completed processing (solved %d locally)\n",
		       computation_time, rank, rank_solved);

		{
			int grid_string_len = get_grid_string_size(sudoku_size);
			int local_output_size_bytes =
				local_num_lines * grid_string_len;
			char *curr_out_ptr;

			local_output_buffer = (char *)malloc(local_output_size_bytes);
			curr_out_ptr = local_output_buffer;

			for (i = 0; i < local_num_lines; ++i) {
				write_grid_to_string(all_grids[i],
						     curr_out_ptr,
						     sudoku_size);
				curr_out_ptr += grid_string_len;
				free_grid(all_grids[i], sudoku_size);
			}
		}

		for (g = 0; g < n_threadgroups; g++)
			free_board(group_states[g].board);
		free(group_states);
		free(thread_groups);
		free(all_grids);
	} else {
		local_output_buffer = (char *)malloc(1);
	}

	/* ------------------------------------------------------------------
	 * Gather results back to rank 0 for verification
	 * ------------------------------------------------------------------ */
	{
		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering results\n",
		       computation_time, rank);

		int *recvcounts = NULL;
		int *rdispls = NULL;
		int my_output_bytes;

		if (rank == 0) {
			recvcounts = (int *)malloc(size * sizeof(int));
			rdispls = (int *)malloc(size * sizeof(int));
		}

		my_output_bytes = (usable_local_threads > 0 && local_num_lines > 0)
			? (int)(local_num_lines * get_grid_string_size(sudoku_size))
			: 0;

		MPI_Gather(&my_output_bytes, 1, MPI_INT,
			   recvcounts, 1, MPI_INT,
			   0, MPI_COMM_WORLD);

		if (rank == 0) {
			int current_disp = 0;
			for (i = 0; i < size; i++) {
				rdispls[i] = current_disp;
				current_disp += recvcounts[i];
			}
			final_output_buffer = (char *)malloc(current_disp + 1);
		}

		MPI_Gatherv(local_output_buffer, my_output_bytes, MPI_CHAR,
			    final_output_buffer, recvcounts, rdispls, MPI_CHAR,
			    0, MPI_COMM_WORLD);

		clock_gettime(CLOCK_MONOTONIC, &ts);
		computation_time = (ts.tv_sec - ts_start.tv_sec) +
			(ts.tv_nsec - ts_start.tv_nsec) / 1e9;
		printf("[%3.6f] - Rank %d - Gathering completed\n",
		       computation_time, rank);

		if (rank == 0) {
			clock_gettime(CLOCK_MONOTONIC, &end_time);
			computation_time =
				(end_time.tv_sec - start_time.tv_sec) +
				(end_time.tv_nsec - start_time.tv_nsec) / 1e9;

			printf("\nTotal computation completed in %.6f seconds.\n",
			       computation_time);

			tot_solved = 0;
			grid_len = get_grid_string_size(sudoku_size);
			current_position = final_output_buffer;
			for (i = 0; i < total_lines; ++i) {
				if (check_solved_string(current_position,
							sudoku_size))
					++tot_solved;
				current_position += grid_len;
			}
			printf("Total correctly solved sudoku puzzles: %d\n",
			       tot_solved);

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
