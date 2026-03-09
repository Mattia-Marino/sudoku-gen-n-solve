#ifndef SOLVER_PARALLEL_H
#define SOLVER_PARALLEL_H

#include <pthread.h>
#include "solver.h"

int parallel_sudoku_solver(struct board *b, int n,
			   int thread_id, int tg_size,
			   pthread_barrier_t *barrier,
			   volatile int *thread_changed);

int parallel_naked_candidates_rows(struct board *b, int n,
			  int **already_propagated, int depth);

int parallel_naked_candidates_cols(struct board *b, int n,
			  int **already_propagated, int depth);

int parallel_naked_candidates_boxes(struct board *b, int n,
			  int **already_propagated, int depth);

#endif /* SOLVER_PARALLEL_H */
