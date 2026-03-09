#define _POSIX_C_SOURCE 200112L

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "../../include/bitarray.h"
#include "../../include/debug.h"
// #include "../../include/solver.h"
#include "../../include/solver_parallel.h"
#include "../../include/sudoku_utils.h"

/* Allocate a 3-D propagation-tracking array (max_depth x n x n). */
static int ***alloc_prop(int max_depth, int n)
{
	int i, j;
	int ***p = (int ***)malloc(max_depth * sizeof(int **));

	for (i = 0; i < max_depth; ++i) {
		p[i] = (int **)malloc(n * sizeof(int *));
		for (j = 0; j < n; ++j)
			p[i][j] = (int *)malloc(n * sizeof(int));
		initialize_propagation_matrix(p[i], n);
	}
	return p;
}

int parallel_sudoku_solver(struct board *b, int n,
			   int thread_id, int tg_size,
			   pthread_barrier_t *barrier,
			   volatile int *thread_changed)
{
	int i, j, t;
	int depth, max_depth;
	int numbers_left;
	int local_is_changed;
	int should_continue;

	/* One tracking matrix per propagation type (matches serial solver) */
	int ***ap_rows, ***ap_cols, ***ap_boxes;

	max_depth = (int)floor((double)n / 2);
	DPRINTF("Thread %d: max_depth = %d\n", thread_id, max_depth);

	ap_rows  = alloc_prop(max_depth, n);
	ap_cols  = alloc_prop(max_depth, n);
	ap_boxes = alloc_prop(max_depth, n);

	/* Display the initial board (thread 1 only) */
	if (thread_id == 1) {
		DPRINTF("\nExtended grid:\n");
		DPRINT_BOARD(b);
	}
	
	/* ---- Main solving loop --------------------------------------- */
	do {
		local_is_changed = 0;

		for (depth = 1; depth <= max_depth; ++depth) {

			/* ===================================================
			 * Thread 1 -> rows   (always)
			 * Thread 2 -> cols   (or thread 1 if tg_size < 2)
			 * Thread 3 -> boxes  (or thread 1 if tg_size < 3)
			 * Each uses its own already_propagated matrix.
			 * =================================================== */
			if (thread_id == 1)
				local_is_changed +=
					naked_candidates_rows(b, n, ap_rows[depth-1], depth);
			
			if ((tg_size >= 2 && thread_id == 2) ||
			    (tg_size < 2 && thread_id == 1))
				local_is_changed +=
					naked_candidates_columns(b, n, ap_cols[depth-1], depth);
			
			if ((tg_size >= 3 && thread_id == 3) ||
			    (tg_size < 3 && thread_id == 1))
				local_is_changed +=
					naked_candidates_boxes(b, n, ap_boxes[depth-1], depth);
		}

		/* Reduce is_changed across all threads */
		thread_changed[thread_id - 1] = local_is_changed;
		/* Barrier 1: ensure all writes to thread_changed are visible */
		pthread_barrier_wait(barrier);

		should_continue = 0;
		for (t = 0; t < tg_size; ++t)
			should_continue |= thread_changed[t];

		/*
		 * Barrier 2: ensure all threads finish reading thread_changed
		 * before any thread loops back and overwrites it in the next
		 * iteration. Without this, a fast thread can write
		 * thread_changed[k] while a slow thread is still reading it,
		 * causing threads to compute different should_continue values
		 * and deadlock on the next barrier.
		 */
		pthread_barrier_wait(barrier);

	} while (should_continue);

	free_propagation_matrix(ap_rows, n);
	free_propagation_matrix(ap_cols, n);
	free_propagation_matrix(ap_boxes, n);

	return 0;
}

int parallel_naked_candidates_rows(struct board *b, int n,
			  int **already_propagated, int depth)
{
	return 0;
}

int parallel_naked_candidates_cols(struct board *b, int n,
			  int **already_propagated, int depth)
{
	return 0;
}

int parallel_naked_candidates_boxes(struct board *b, int n,
			  int **already_propagated, int depth)
{
	return 0;
}