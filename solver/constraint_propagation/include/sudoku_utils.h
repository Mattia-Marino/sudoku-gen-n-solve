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
 * Reads a Sudoku puzzle from a string line and populates the grid.
 *
 * @param grid Pre-allocated grid to store the puzzle
 * @param line The string containing space-separated integers
 * @param n The size of the grid
 * @returns 0 on success, -1 on error
 */
int read_grid_from_string(int **grid, const char *line, int n);

/**
 * Converts a Sudoku grid to a string line format.
 *
 * @param grid The grid to convert
 * @param line Buffer to store the resulting string (must be pre-allocated)
 * @param n The size of the grid
 * @returns 0 on success, -1 on error
 */
int write_grid_to_string(int **grid, char *line, int n);

/**
 * Computes the maximum size needed to store the grid as a string.
 * Accounts for spaces, the trailing newline, and the null terminator.
 * * @param n The dimension of the Sudoku (e.g., 9 for 9x9).
 * @return The total number of bytes needed for the string buffer.
 */
size_t get_grid_string_size(int n);

/**
 * Check if the sudoku has been completely solved.
 *
 * @param grid The sudoku grid
 * @param n The size of the sudoku
 * @return int - 1 if solved, else 0
 */
int check_solved(int **grid, int n);

/**
 * Check if a sudoku string represents a completely solved puzzle.
 *
 * @param line The string containing space-separated integers representing the sudoku
 * @param n The size of the sudoku (dimension)
 * @return int - 1 if solved (no zeros found), 0 if unsolved, -1 on error
 */
int check_solved_string(const char *line, int n);

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