#ifndef SOLVER_P_H
#define SOLVER_P_H

#include "../include/common.h"
#include "../include/linked_list.h"


int sudoku_solver_parallel(int **grid, int n, int rank, int size);

struct node ***extend_grid_parallel(int **grid, int n);

void initialize_propagation_matrix_parallel(int **matrix, int n);

/* Naked candidates */

int naked_candidates_rows_parallel(struct node ***extended_grid, int n,
			  int **already_propagated, int depth, int start_row, int end_row);

int naked_candidates_columns_parallel(struct node ***extended_grid, int n,
			     int **already_propagated, int depth, int start_col, int end_col);

int naked_candidates_boxes_parallel(struct node ***extended_grid, int n,
			   int **already_propagated, int depth, int start_box, int end_box);

/* Propagations for naked candidates */

void propagate_row_parallel(struct node ***extended_grid, int n,
		   struct coordinates *coord, int n_coordinates, int value);

void propagate_column_parallel(struct node ***extended_grid, int n,
		      struct coordinates *coord, int n_coordinates, int value);

void propagate_box_parallel(struct node ***extended_grid, int n,
		   struct coordinates *coord, int n_coordinates, int value);

/*Hidden singles*/

int hidden_singles_parallel(struct node ***extended_grid, int n, int start_row, int end_row);

int check_hidden_single_parallel(struct node ***extended_grid, int n, int row, int col,
			int value);

void print_extended_grid_parallel(struct node ***extended_grid, int n);

void free_extended_grid_parallel(struct node ***extended_grid, int n);

void free_propagation_matrix_parallel(int ***propagation, int n);

#endif /* SOLVER_P_H */
