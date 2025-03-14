/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SUDOKU_H
#define SUDOKU_H

#include <stdio.h>
#include <stdlib.h>

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
 * @brief Inserts the first row of the Sudoku grid with a random permutation.
 *
 * @param sudoku Pointer to the Sudoku puzzle to modify
 */
void insertFirstLine(struct Sudoku *sudoku);

/**
 * @brief Solves the Sudoku puzzle using Algorithm X.
 *
 * @param sudoku Pointer to the Sudoku puzzle to solve
 * @return 1 if the puzzle is solved, 0 otherwise
 */
int hasUniqueSolution(struct Sudoku *sudoku, struct Sudoku *solution);

/**
 * @brief Remove the numbers from the board until we reach minimal solution.
 *
 * @param sudoku Pointer to the Sudoku puzzle to properly generate
 */
void removeNumbers(struct Sudoku *sudoku);

/**
 * @brief Saves the Sudoku grid to a file named output.txt
 *
 * @param sudoku Pointer to the Sudoku puzzle to save
 */
void saveSudokuToFile(struct Sudoku *sudoku);

/**
 * @brief Deallocates the memory used by a Sudoku puzzle.
 *
 * @param sudoku Pointer to the Sudoku puzzle to destroy
 */
void destroySudoku(struct Sudoku *sudoku);

#endif /* SUDOKU_H */
