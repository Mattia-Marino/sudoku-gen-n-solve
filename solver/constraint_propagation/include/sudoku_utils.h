/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SUDOKU_UTILS_H
#define SUDOKU_UTILS_H

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
 * Reads a Sudoku puzzle from a line of text and populates the grid.
 *
 * @param grid Pre-allocated grid to store the puzzle
 * @param file The file to read the grid from
 * @param n The size of the grid
 * @returns 0 if read goes alright, 1 if reached EOF, -1 if error
 */
int read_grid_from_file(int **grid, FILE *file, int n);

/**
 * Check if the sudoku has been completely solved.
 *
 * @param grid The sudoku grid
 * @param n The size of the sudoku
 * @return int - 1 if solved, else 0
 */
int check_solved(int **grid, int n);

/**
 * Displays a Sudoku grid to standard output.
 *
 * @param grid The grid to display
 * @param n The size of the grid
 */
void display_sudoku(int **grid, int n);

/**
 * @brief Counts the number of lines in a given file and resets the file pointer to the beginning.
 *
 * @param filename The path to the file to be processed.
 * @return The number of lines in the file, or -1 if the file cannot be opened.
 */
int count_lines_in_file(FILE *file);

#endif /* SUDOKU_UTILS_H */