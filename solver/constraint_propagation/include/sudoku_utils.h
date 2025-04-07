/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SUDOKU_UTILS_H
#define SUDOKU_UTILS_H

/**
 * Reads the size of a Sudoku grid from a file.
 *
 * @param filename Path to the file containing the Sudoku puzzle
 * @returns The size (n) of the Sudoku grid, or -1 on error
 */
int read_size_from_file(const char *filename);

/**
 * Dynamically allocates memory for an n x n Sudoku grid.
 *
 * @param n Size of the Sudoku grid
 * @returns Pointer to the allocated grid, or NULL if allocation fails
 */
int **create_grid(int n);

/**
 * Frees the memory allocated for a Sudoku grid.
 *
 * @param grid The grid to be freed
 * @param n The size of the grid
 */
void free_grid(int **grid, int n);

/**
 * Reads a Sudoku puzzle from a file into a grid.
 *
 * @param grid Pre-allocated grid to store the puzzle
 * @param filename Path to the file containing the Sudoku puzzle
 * @returns 1 on success, 0 on failure
 */
int read_grid_from_file(int **grid, const char *filename);

/**
 * Displays a Sudoku grid to standard output.
 *
 * @param grid The grid to display
 * @param n The size of the grid
 */
void display_sudoku(int **grid, int n);

#endif /* SUDOKU_UTILS_H */