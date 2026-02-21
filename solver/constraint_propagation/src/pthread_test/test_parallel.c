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
#include "../../include/solver_parallel.h"

void *outputs[15];

int ***all_grids;	/* All the grids assigned to the computational node */
int sudoku_size;	/* The size of the sudoku */

typedef void *( *target )( void *);

typedef struct {
	pthread_barrier_t barrier_ready;
	pthread_barrier_t barrier_done;
	volatile int data_is_over;
} tg_sync;

tg_sync *group_syncs;		/* Per-threadgroup synchronization, indexed by tg_id */
int ***common_grids;		/* Per-threadgroup shared grid, indexed by tg_id */

typedef struct {
	int tg_id;		/* Threadgroup ID */
	int thread_id;		/* Thread ID within the group (1-based) */
	int tg_size;		/* Number of threads in this threadgroup */
	int start;		/* Start index in all_grids (inclusive) */
	int end;		/* End index in all_grids (inclusive) */
} thread_arg;

/* Worker thread function, solves sudoku puzzles in all_grids in-place */
void *pthreads_solver(void *arg)
{
	int i;
	thread_arg *ta = (thread_arg *) arg;
	int tg_id = ta->tg_id;
	int thread_id = ta->thread_id;
	tg_sync *sync = &group_syncs[tg_id];

	printf("Threadgroup %d - Thread %d is ready (group size: %d)\n",
		   tg_id, thread_id, ta->tg_size);

	if (thread_id == 1) {
		/* Provider thread: feed puzzles to the group */
		printf("\tThreadgroup %d - Range is [%d - %d]\n",
			   tg_id, ta->start, ta->end);

		for (i = ta->start; i <= ta->end; ++i) {
			/* Load the next puzzle into the shared grid */
			common_grids[tg_id] = all_grids[i];

			/* Signal all threads that data is ready */
			pthread_barrier_wait(&sync->barrier_ready);

			/* Process (display) the sudoku */
			printf("\n\nThreadgroup %d - Thread %d got this sudoku:\n",
				   tg_id, thread_id);
			display_sudoku(common_grids[tg_id], sudoku_size);

			/* Wait for all threads to finish processing */
			pthread_barrier_wait(&sync->barrier_done);
		}

		/* Signal that there is no more data */
		sync->data_is_over = 1;
		pthread_barrier_wait(&sync->barrier_ready);
	} else {
		/* Worker threads: wait for data, process, repeat */
		while (1) {
			/* Wait for the provider to load data */
			pthread_barrier_wait(&sync->barrier_ready);

			/* Check if all puzzles have been processed */
			if (sync->data_is_over)
				break;

			/* Process (display) the sudoku */
			printf("\n\nThreadgroup %d - Thread %d got this sudoku:\n",
				   tg_id, thread_id);
			display_sudoku(common_grids[tg_id], sudoku_size);

			/* Signal that processing is done */
			pthread_barrier_wait(&sync->barrier_done);
		}
	}

	free(ta);
	return NULL;
}

