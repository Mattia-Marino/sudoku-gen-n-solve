/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SUDOKU_H
#define SUDOKU_H

/**
 * @struct Sudoku
 * @brief Structure representing a Sudoku puzzle.
 */
struct Sudoku {
	int **grid;		/* 2D array representing the Sudoku grid */
	int size;		/* Size of the Sudoku grid (e.g., 9, 16, 25) */
	int squareRootOfSize;	/* Square root of size (e.g., 3, 4, 5) */
};

/**
 * @brief Initializes a new Sudoku puzzle of the specified size.
 *
 * @param size The size of the Sudoku grid (should be a perfect square)
 * @return Pointer to the initialized Sudoku structure
 */
struct Sudoku *initSudoku(int size);

/**
 * @brief Displays the Sudoku grid in a formatted way.
 *
 * @param sudoku Pointer to the Sudoku puzzle to display
 */
void displaySudoku(struct Sudoku *sudoku);

/**
 * @brief Creates a new grid of the specified size.
 *
 * @param n The size of the grid
 * @return Pointer to the created grid
 */
int **createGrid(int n);

/**
 * @brief Frees the memory allocated for a grid.
 *
 * @param grid The grid to free
 * @param n The size of the grid
 */
void freeGrid(int **grid, int n);

/**
 * @brief Prints a grid in a formatted way.
 *
 * @param grid The grid to print
 * @param n The size of the grid
 */
void printGrid(int **grid, int n);

/**
 * @brief Reads the size of a Sudoku grid from a file.
 *
 * @param filename The name of the file to read from
 * @return The size of the Sudoku grid
 */
int readSizeFromFile(const char *filename);

/**
 * @brief Reads a Sudoku grid from a file.
 *
 * @param sudoku Pointer to the Sudoku structure to fill
 * @param filename The name of the file to read from
 */
void readGridFromFile(struct Sudoku *sudoku, const char *filename);

/**
 * @brief Deallocates the memory used by a Sudoku puzzle.
 *
 * @param sudoku Pointer to the Sudoku puzzle to destroy
 */
void destroySudoku(struct Sudoku *sudoku);

#endif /* SUDOKU_H */
