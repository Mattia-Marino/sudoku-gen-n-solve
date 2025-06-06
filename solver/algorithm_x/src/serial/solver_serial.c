/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/solver_comm.h"
#include "../../include/serial/solver_serial.h"
#include "../../include/sudoku.h"
#include "../../include/Dancing-Links/dancing-links.h"

/*
 * Algorithm X of Donald Knuth.
 * Recursive function for searching the solution of the Exact Cover Problem.
 * Uses depth-first search with backtracking.
 */
void search(struct ExactCover *ex_cover, struct Sudoku *sudoku, int k)
{
	struct node *col;	/* Column header */
	struct node *i;		/* Node */

	/* If all columns are covered, a solution is found */
	if (ex_cover->head == ex_cover->head->right) {
		/* Mark the Exact Cover as solved and return */
		ex_cover->isSolved = 1;
		return;
	}

	/* Choose a column deterministically (e.g., the first non-covered column) */
	col = chooseColumn(ex_cover->head);

	/* Cover the chosen column */
	cover(col);

	/* Iterate through each row that intersects with the chosen column */
	for (i = col->down; i != col && !ex_cover->isSolved; i = i->down) {
		struct node *j;

		/* Include this row in the partial solution */
		ex_cover->answer[k] = i;

		/* Cover all columns that intersect with this row */
		for (j = i->right; j != i; j = j->right)
			cover(j->colHead);

		/* Recursively search for a solution with the updated partial solution */
		search(ex_cover, sudoku, k + 1);

		/* If a solution is found, return */
		if (ex_cover->isSolved)
			return;

		/* Backtrack: Reset the partial solution and uncover the columns */
		i = ex_cover->answer[k];
		ex_cover->answer[k] = NULL;
		col = i->colHead;
		for (j = i->left; j != i; j = j->left)
			uncover(j->colHead);
	}

	/* Uncover the chosen column */
	uncover(col);
}

/* Function for solving the Sudoku */
int SudokuSolver(struct Sudoku *sudoku)
{
	struct ExactCover *ex_cover;

	/* Initialize the Exact Cover Problem */
	ex_cover = initExactCover(sudoku);

	/* Create the sparse matrix representation of the Sudoku */
	makeSparseMatrix(ex_cover);

	/* Create the Torodial Double Linked List from the sparse matrix */
	makeTorodialDList(ex_cover);

	/* Transform the Torodial Double Linked List based on the Sudoku Grid */
	transformTorodialDList(ex_cover, sudoku);

	/* Search for a solution to the Exact Cover Problem */
	search(ex_cover, sudoku, 0);

	/* If no solution is found, print a message and return 0 */
	if (!ex_cover->isSolved) {
		printf("No solution found.\n");
		return 0;
	}

	/* Map the solution to the original sudoku: */
	MapAnswer(ex_cover, sudoku);

	/* Destroy the Exact Cover Problem */
	destroyExactCover(ex_cover);
	return 1;
}
