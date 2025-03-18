/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SOLVER_H
#define SOLVER_H

#include "sudoku.h"
#include "Dancing-Links/dancing-links.h"

/**
 * @struct ExactCover
 * @brief Structure representing the Exact Cover problem for Sudoku solving
 */
struct ExactCover {
	struct node *head;		/* Head of the toroidal doubly linked list */
	struct node **answer;		/* Array to store the solution */
	struct node **original;		/* Array to store the original values */
	struct node **deallocate;	/* Array of nodes to deallocate */
	int **matrix;		/* Sparse matrix representation */
	int row;		/* Number of rows */
	int col;		/* Number of columns */
	int size;		/* Size of the Sudoku puzzle */
	int isSolved;		/* Flag to indicate if the puzzle is solved */
	int count;		/* Count of nodes to deallocate */
};

/**
 * @brief Initializes an ExactCover structure for the Sudoku puzzle.
 *
 * @param sudoku The Sudoku puzzle to create the ExactCover for
 * @return Pointer to the initialized ExactCover structure
 */
struct ExactCover *initExactCover(struct Sudoku *sudoku);

/**
 * @brief Destroys the ExactCover structure and frees memory.
 *
 * @param ex_cover The ExactCover structure to destroy
 */
void destroyExactCover(struct ExactCover *ex_cover);

/**
 * @brief Creates the sparse matrix representation of the Exact Cover problem.
 *
 * @param ex_cover The ExactCover structure to populate
 */
void makeSparseMatrix(struct ExactCover *ex_cover);

/**
 * @brief Creates the toroidal double linked list from the sparse matrix.
 *
 * @param ex_cover The ExactCover structure containing the sparse matrix
 */
void makeTorodialDList(struct ExactCover *ex_cover);

/**
 * @brief Transforms the toroidal double linked list based on the Sudoku grid.
 *
 * @param ex_cover The ExactCover structure to transform
 * @param sudoku The Sudoku puzzle with initial values
 */
void transformTorodialDList(struct ExactCover *ex_cover, struct Sudoku *sudoku);

/**
 * @brief Maps the answer from the Exact Cover problem to the Sudoku grid.
 *
 * @param ex_cover The ExactCover structure containing the solution
 * @param sudoku The Sudoku puzzle to update with the solution
 */
void MapAnswer(struct ExactCover *ex_cover, struct Sudoku *sudoku);

/**
 * @brief Recursively searches for a solution to the Exact Cover problem.
 *
 * @param ex_cover The ExactCover structure to search
 * @param sudoku The Sudoku puzzle
 * @param k The current depth in the search
 */
void search(struct ExactCover *ex_cover, struct Sudoku *sudoku, int k);

/**
 * @brief Solves a Sudoku puzzle using Dancing Links algorithm.
 *
 * @param sudoku Pointer to the Sudoku puzzle to be solved
 */
int SudokuSolver(struct Sudoku *sudoku);

#endif /* SOLVER_H */
