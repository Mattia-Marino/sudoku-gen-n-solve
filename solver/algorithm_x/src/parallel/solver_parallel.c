/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/solver_comm.h"
#include "../../include/parallel/solver_parallel.h"
#include "../../include/sudoku.h"
#include "../../include/Dancing-Links/dancing-links.h"

#define DEBUG_ALGO_X 1

/*
 * Algorithm X of Donald Knuth.
 * Recursive function for searching the solution of the Exact Cover Problem.
 * Uses depth-first search with backtracking.
 */
void search(struct ExactCover *ex_cover, struct Sudoku *sudoku, int k)
{
	struct node *col; /* Column header */
	struct node *i; /* Node */
	char indent[1000] = "";
	int j;

	/* Create an indent string based on recursion depth */
	for (j = 0; j < k; j++)
		strcat(indent, "  ");

	if (DEBUG_ALGO_X) printf("%s[Depth %d] Searching...\n", indent, k);

	/* If all columns are covered, a solution is found */
	if (ex_cover->head == ex_cover->head->right) {
		if (DEBUG_ALGO_X) printf("%s[Depth %d] SOLUTION FOUND!\n", indent, k);
		/* Mark the Exact Cover as solved and return */
		ex_cover->isSolved = 1;
		return;
	}

	/* Choose a column deterministically (e.g., the first non-covered column) */
	col = chooseColumn(ex_cover->head);
	if (DEBUG_ALGO_X) printf("%s[Depth %d] Selected column with %d rows\n", indent, k,
	       col->size);

	/* Cover the chosen column */
	if (DEBUG_ALGO_X) printf("%s[Depth %d] Covering column\n", indent, k);
	cover(col);

	/* Iterate through each row that intersects with the chosen column */
	int row_count = 0;
	for (i = col->down; i != col && !ex_cover->isSolved; i = i->down) {
		struct node *j;
		row_count++;

		if (DEBUG_ALGO_X) printf("%s[Depth %d] Trying row %d (value %d at position [%d,%d])\n",
		       indent, k, row_count, i->id[0], i->id[1], i->id[2]);

		/* Include this row in the partial solution */
		ex_cover->answer[k] = i;

		/* Cover all columns that intersect with this row */
		int covered_cols = 0;
		for (j = i->right; j != i; j = j->right) {
			cover(j->colHead);
			covered_cols++;
		}
		if (DEBUG_ALGO_X) printf("%s[Depth %d] Covered %d additional columns\n", indent,
		       k, covered_cols);

		/* Recursively search for a solution with the updated partial solution */
		search(ex_cover, sudoku, k + 1);

		/* If a solution is found, return */
		if (ex_cover->isSolved) {
			if (DEBUG_ALGO_X) printf("%s[Depth %d] Solution propagated up\n", indent,
			       k);
			return;
		}

		/* Backtrack: Reset the partial solution and uncover the columns */
		if (DEBUG_ALGO_X) printf("%s[Depth %d] Backtracking - this row didn't work\n",
		       indent, k);
		i = ex_cover->answer[k];
		ex_cover->answer[k] = NULL;
		col = i->colHead;

		int uncovered_cols = 0;
		for (j = i->left; j != i; j = j->left) {
			uncover(j->colHead);
			uncovered_cols++;
		}
		if (DEBUG_ALGO_X) printf("%s[Depth %d] Uncovered %d columns\n", indent, k,
		       uncovered_cols);
	}

	/* Uncover the chosen column */
	if (DEBUG_ALGO_X) printf("%s[Depth %d] No solution found with this column, uncovering it\n",
	       indent, k);
	uncover(col);
	if (DEBUG_ALGO_X) printf("%s[Depth %d] Backtracking to try different column\n", indent, k);
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
