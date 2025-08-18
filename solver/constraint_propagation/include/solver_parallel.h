#ifndef SOLVER_PARALLEL_H
#define SOLVER_PARALLEL_H

#include "linked_list.h" /* For struct node */
#include "solver.h"	/* For struct coordinates */

int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col);

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row);


/**
 * Checks if a value is present in the linked list.
 * 
 * @param head The head of the linked list.
 * @param value The value to search for.
 * @return 1 if the value is found, 0 otherwise.
 */
int has_value(struct node *head, int value);			

/** 
 * Checks if a cell (r, c) is one of the cells in the provided tuple (coord array).
 *
 * @param r The row of the cell to check.
 * @param c The column of the cell to check.
 * @param coord The coordinates array representing the naked tuple.
 * @param n_coordinates The number of coordinates in the naked tuple.
 * @return 1 if the coordinate is found, 0 otherwise.
 */
int is_coord_in_tuple(int r, int c, struct coordinates *coord, int n_coordinates);

/**  
 * Propagates (removes) a specific value from candidates in a row,
 * excluding cells that are part of the 'naked tuple'.
 *
 * @param extended_grid The Sudoku grid with candidate lists.
 * @param n The size of the grid.
 * @param coord The coordinates of the cells in the naked tuple.
 * @param n_coordinates The number of coordinates in the naked tuple.
 * @param value_to_propagate The value to remove from candidates.
 * @return the number of candidates removed.
 */
int parallel_propagate_row(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate);

/** 
 * Propagates (removes) a specific value from candidates in a column,
 * excluding cells that are part of the 'naked tuple'.
 * 
 * @param extended_grid The Sudoku grid with candidate lists.
 * @param n The size of the grid.
 * @param coord The coordinates of the cells in the naked tuple.
 * @param n_coordinates The number of coordinates in the naked tuple.
 * @param value_to_propagate The value to remove from candidates.
 * @return the number of candidates removed.
 */
int parallel_propagate_column(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate);

/** 
 * Propagates (removes) a specific value from candidates in a box,
 * excluding cells that are part of the 'naked tuple'.
 * 
 * @param extended_grid The Sudoku grid with candidate lists.
 * @param n The size of the grid.
 * @param coord The coordinates of the cells in the naked tuple.
 * @param n_coordinates The number of coordinates in the naked tuple.
 * @param value_to_propagate The value to remove from candidates.
 * @return the number of candidates removed.
 */
int parallel_propagate_box(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate);

/**
 * Finds and resolves hidden singles within the extended grid.
 * 
 * @param extended_grid The Sudoku grid with candidate lists.
 * @param n The size of the grid.
 * @return the total number of candidates eliminated.
 */
int parallel_hidden_singles(struct node ***extended_grid, int n);

#endif /* SOLVER_PARALLEL_H */