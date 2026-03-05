/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SOLVER_H
#define SOLVER_H

struct board {
	int n;
	int **cells;
};

struct coordinates {
	int row;
	int column;
};

int sudoku_solver(int **grid, int n);

/**
 * Creates and returns a board of size n x n.
 * Each cell is a bitarray (int) initialized to 0.
 *
 * @param n Size of the sudoku (default 9)
 * @return Pointer to the newly allocated board
 */
struct board *init_board(int n);

/**
 * Populates the board cells from a sudoku grid.
 * If grid[i][j] == 0, all candidate bits (1..n) are set to 1.
 * If grid[i][j] == k, only bit k is set to 1.
 *
 * @param b The board to populate
 * @param grid The n x n sudoku grid
 */
void populate_board(struct board *b, int **grid);

/**
 * Prints the board, displaying each cell as its bitarray representation.
 * Bits are printed from position 1 to n (left to right).
 *
 * @param b The board to print
 */
void print_board(const struct board *b);

/**
 * Converts the board back to a sudoku grid.
 * If a cell has exactly one candidate, the corresponding number is placed
 * in the grid. Otherwise, 0 is placed in the grid.
 *
 * @param b The board to convert
 * @param grid The n x n grid to write into
 */
void board_to_grid(const struct board *b, int **grid);

/**
 * Frees the memory allocated for a board.
 *
 * @param b The board to free
 */
void free_board(struct board *b);

void initialize_propagation_matrix(int **matrix, int n);

void free_propagation_matrix(int ***propagation, int n);

int naked_candidates_rows(struct board *b, int n,
			  int **already_propagated, int depth);

int naked_candidates_columns(struct board *b, int n,
			  int **already_propagated, int depth);

int naked_candidates_boxes(struct board *b, int n,
			  int **already_propagated, int depth);

void propagate_row(struct board *b, int n, struct coordinates *coord, 
		   	  int n_coordinates, int value);

void propagate_column(struct board *b, int n, struct coordinates *coord, 
		   	  int n_coordinates, int value);

void propagate_box(struct board *b, int n, struct coordinates *coord, 
		   	  int n_coordinates, int value);

#endif
