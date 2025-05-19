#ifndef SOLVER_PARALLEL_H
#define SOLVER_PARALLEL_H

#include "solver.h"

int parallel_sudoku_solver(int **grid, int n, int rank, int size);

int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col);

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);

void communication(struct node ***extended_grid, int n, int rank, int size);
#endif /* SOLVER_PARALLEL_H */
