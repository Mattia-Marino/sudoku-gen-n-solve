/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SUDOKU_UTILS_H
#define SUDOKU_UTILS_H

/**
 * Structure to hold multiple Sudoku grids
 */
typedef struct {
    int ***grids;       /* Array of grid pointers */
    int count;          /* Number of grids */
    int n;              /* Size of each grid (n x n) */
    int capacity;       /* Current capacity of the grids array */
    struct node ****extended_grids; /* Array of extended grid pointers */
} sudoku_collection_t;

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
 * Creates and initializes a sudoku collection structure.
 *
 * @param n The size of each grid (n x n)
 * @param initial_capacity Initial capacity for the collection
 * @returns Pointer to the allocated collection, or NULL if allocation fails
 */
sudoku_collection_t *create_sudoku_collection(int n, int initial_capacity);

/**
 * Reads all Sudoku grids from a file and stores them in a collection.
 *
 * @param filename The file to read from
 * @param n The size of each grid
 * @returns Pointer to the sudoku collection, or NULL if failed
 */
sudoku_collection_t *read_all_sudokus_from_file(const char *filename, int n);

/**
 * Prints all grids in the sudoku collection.
 *
 * @param collection The sudoku collection
 */
void display_sudoku_collection(sudoku_collection_t *collection);

/**
 * Frees the memory allocated for a sudoku collection.
 *
 * @param collection The collection to be freed
 */
void free_sudoku_collection(sudoku_collection_t *collection);

/**
 * Gets a specific grid from the collection.
 *
 * @param collection The sudoku collection
 * @param index The index of the grid to retrieve
 * @returns Pointer to the grid, or NULL if index is invalid
 */
int **get_grid_from_collection(sudoku_collection_t *collection, int index);

/**
 * Gets the extended version of a specific grid from the collection.
 *
 * @param collection The sudoku collection
 * @param index The index of the grid to retrieve
 * @returns the extended grid in the form of a matrix
 */
struct node ***get_extended_grid_from_collection(sudoku_collection_t *collection, int index);


/**
 * Converts an extended grid (with candidate lists) back to the original grid.
 *
 * @param extended_grid The extended grid with candidate lists
 * @param n The size of the grid
 * @returns Pointer to the original grid, or NULL if conversion fails
 */
int **convert_extended_to_original_grid(struct node ***extended_grid, int n);

#endif /* SUDOKU_UTILS_H */