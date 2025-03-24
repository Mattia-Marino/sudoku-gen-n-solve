/* SPDX-License-Identifier: GPL-3.0 */

#ifndef SOLVER_H
#define SOLVER_H

#include "../solver_comm.h"
#include "../sudoku.h"
#include "../Dancing-Links/dancing-links.h"

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
