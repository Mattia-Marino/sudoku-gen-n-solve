#ifndef SOLVER_PARALLEL_H
#define SOLVER_PARALLEL_H

#include <pthread.h>
#include "solver.h"

int parallel_sudoku_solver(struct node ***extended_grid, int n,
			   int thread_id, int tg_size,
			   pthread_barrier_t *barrier,
			   volatile int *shared_is_changed);

int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col);

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);

#endif /* SOLVER_PARALLEL_H */
