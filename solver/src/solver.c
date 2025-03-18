/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/solver.h"
#include "../include/sudoku.h"
#include "../include/Dancing-Links/dancing-links.h"

/*
 * Function for initializing the Exact Cover Problem.
 * Takes a Sudoku puzzle and creates the data structures needed for solving it
 * as an exact cover problem.
 */
struct ExactCover *initExactCover(struct Sudoku *sudoku)
{
	int i; /* Loop counter */
	int sizeFactor; /* Factor to scale memory allocations */
	struct ExactCover *ex_cover; /* Exact Cover struct */

	/* Allocate memory for the ExactCover struct and set head to NULL */
	ex_cover = malloc(sizeof(struct ExactCover));
	ex_cover->head = NULL;

	/*
	 * Scale memory allocations based on puzzle size.
	 * Larger puzzles require significantly more memory.
	 */
	sizeFactor = sudoku->size >= 25 ? 100 : (sudoku->size >= 16 ? 20 : 1);

	/*
	 * Allocate arrays to store:
	 * - answer: holds the rows that form the solution
	 * - original: holds the rows representing the initial puzzle state
	 */
	ex_cover->answer = malloc(500 * sizeFactor * sizeof(struct node *));
	ex_cover->original = malloc(500 * sizeFactor * sizeof(struct node *));

	/* Initialize to NULL */
	for (i = 0; i < 500 * sizeFactor; i++) {
		ex_cover->answer[i] = NULL;
		ex_cover->original[i] = NULL;
	}

	/*
	 * Set row and column sizes for the sparse matrix
	 * For n×n Sudoku: rows = n³, columns = 4n²
	 * Each row represents one possibility (digit in a specific cell)
	 * Each column represents one constraint
	 */
	ex_cover->row = sudoku->size * sudoku->size * sudoku->size;
	ex_cover->col = 4 * sudoku->size * sudoku->size;

	/*
	 * Allocate memory for node tracking
	 * The deallocate array stores pointers to all allocated nodes
	 * for easy cleanup later
	 */
	ex_cover->deallocate = malloc(4 * ex_cover->row * ex_cover->col *
				      sizeof(struct node *));
	for (i = 0; i < 4 * ex_cover->row * ex_cover->col; i++)
		ex_cover->deallocate[i] = NULL; /* Initialize to NULL */

	/*
	 * Allocate memory for the sparse matrix representation
	 * This matrix encodes the exact cover problem constraints
	 */
	ex_cover->matrix = malloc(ex_cover->row * sizeof(int *));
	for (i = 0; i < ex_cover->row; i++) {
		ex_cover->matrix[i] = malloc(ex_cover->col * sizeof(int));
		memset(ex_cover->matrix[i], 0, ex_cover->col * sizeof(int));
	}

	/* Set final properties of the exact cover structure */
	ex_cover->size = sudoku->size;
	ex_cover->isSolved = 0;
	ex_cover->count = 0;

	return ex_cover;
}

/*
 * Function to destroy the ExactCover struct.
 * Releases all allocated memory to prevent leaks.
 */
void destroyExactCover(struct ExactCover *ex_cover)
{
	int i;

	/* Safety check for NULL pointer */
	if (ex_cover == NULL)
		return;

	/* Free all nodes in the deallocate array */
	for (i = 0; i < ex_cover->count && ex_cover->deallocate[i] != NULL;
	     i++) {
		free(ex_cover->deallocate[i]);
	}

	/* Free the solution-tracking arrays */
	free(ex_cover->answer);
	free(ex_cover->original);
	free(ex_cover->deallocate);

	/* Free the sparse matrix */
	for (i = 0; i < ex_cover->row; i++)
		free(ex_cover->matrix[i]);
	free(ex_cover->matrix);

	/* Free the structure itself */
	free(ex_cover);
}

/*
 * Function for creating the Sparse Matrix of the Exact Cover Problem.
 * Creates a binary matrix where:
 * - Each row represents a possible digit placement (row, column, value)
 * - Each column represents a constraint that must be satisfied
 */
void makeSparseMatrix(struct ExactCover *ex_cover)
{
	int col;		/* Column index */
	int row;		/* Row index */
	int col_offset;		/* Offset for different constraint sections */
	int adjusted_col;	/* Relative column index within constraint section */
	int col_block;		/* Block index within constraint section */
	int x;			/* Matrix entry x-coordinate */
	int y;			/* Matrix entry y-coordinate */
	int s_size;		/* Size of subgrid (square root of puzzle size) */

	/*
	 * Constraint 1: Row-Column constraint
	 * Each cell must contain exactly one number
	 * First n² columns, where n is the Sudoku size
	 */
	for (col = 0; col < ex_cover->size * ex_cover->size; col++)
		for (row = 0; row < ex_cover->size; row++)
			ex_cover->matrix[row + col * ex_cover->size][col] = 1;

	/*
	 * Constraint 2: Row-Number constraint
	 * Each row must contain each number exactly once
	 * Next n² columns
	 */
	col_offset = ex_cover->size * ex_cover->size;

	for (col = ex_cover->size * ex_cover->size;
	     col < 2 * ex_cover->size * ex_cover->size; col++) {
		adjusted_col = col % (ex_cover->size * ex_cover->size);
		col_block = adjusted_col / ex_cover->size;

		for (row = 0; row < ex_cover->size; row++) {
			x = row + (col_block * ex_cover->size) + col_offset;
			y = adjusted_col * ex_cover->size + row;

			ex_cover->matrix[y][x] = 1;
		}
	}

	/*
	 * Constraint 3: Column-Number constraint
	 * Each column must contain each number exactly once
	 * Next n² columns
	 */
	col_offset = 2 * ex_cover->size * ex_cover->size;
	for (col = 2 * ex_cover->size * ex_cover->size;
	     col < 3 * ex_cover->size * ex_cover->size; col++) {
		adjusted_col = col % (ex_cover->size * ex_cover->size);

		for (row = 0; row < ex_cover->size; row++) {
			x = (row + adjusted_col * ex_cover->size) %
				    (ex_cover->size * ex_cover->size) +
			    col_offset;
			y = adjusted_col * ex_cover->size + row;

			ex_cover->matrix[y][x] = 1;
		}
	}

	/*
	 * Constraint 4: Box-Number constraint
	 * Each box must contain each number exactly once
	 * Final n² columns
	 */
	s_size = sqrt(ex_cover->size);

	col_offset = 3 * ex_cover->size * ex_cover->size;
	for (col = 3 * ex_cover->size * ex_cover->size;
	     col < 4 * ex_cover->size * ex_cover->size; col++) {
		adjusted_col = col % (ex_cover->size * ex_cover->size);
		col_block = (adjusted_col / s_size) % s_size;

		for (row = 0; row < ex_cover->size; row++) {
			x = col_offset + row +
			    (col_block +
			     (adjusted_col / (ex_cover->size * s_size)) *
				     s_size) *
				    ex_cover->size;
			y = adjusted_col * ex_cover->size + row;

			ex_cover->matrix[y][x] = 1;
		}
	}
}

/*
 * Function for creating the Toroidal Double Linked List from the Sparse Matrix.
 * Implements Knuth's Dancing Links (DLX) data structure for efficient backtracking.
 */
void makeTorodialDList(struct ExactCover *ex_cover)
{
	struct node *head;	/* Root node of the structure */
	struct node *temp;	/* Temporary node for building links */
	int i;			/* Row iterator */
	int j;			/* Column iterator */
	int id[3];		/* Node identifier [value, row, column] */

	/* Create the header node */
	head = malloc(sizeof(struct node));

	/* Add head to deallocate array for memory management */
	ex_cover->deallocate[ex_cover->count++] = head;

	/* Initialize header node as self-referencing links */
	head->left = head;
	head->right = head;
	head->up = head;
	head->down = head;
	head->size = -1;
	head->colHead = head;

	temp = head;

	/* Create column header nodes and link them horizontally */
	for (i = 0; i < ex_cover->col; i++) {
		struct node *node = malloc(sizeof(struct node));

		ex_cover->deallocate[ex_cover->count++] = node;
		node->left = temp;		/* Link to previous column */
		node->right = head;		/* Link back to header */
		node->up = node;		/* Self-reference vertically */
		node->down = node;
		node->colHead = node;		/* Column header points to itself */
		node->size = 0;			/* Initialize size counter */
		temp->right = node;		/* Update previous node's right link */
		temp = node;			/* Move to new node */
	}

	/* Initialize node ID for the first entry */
	id[0] = 0;	/* Value (1-n) */
	id[1] = 1;	/* Row (1-indexed) */
	id[2] = 1;	/* Column (1-indexed) */

	/* Create nodes for each 1 in the sparse matrix and link them */
	for (i = 0; i < ex_cover->row; i++) {
		struct node *cur;	/* Current column we're examining */
		struct node *prev;	/* Previous node in the current row */

		cur = head->right;	/* Start with the first column */
		prev = NULL;		/* No previous node yet */

		/*
		 * Update node ID based on position
		 * This maps matrix rows to Sudoku (value, row, column) triples
		 */
		if (i != 0 && i % (ex_cover->size * ex_cover->size) == 0) {
			/* Moving to the next row in the Sudoku */
			id[0] -= ex_cover->size - 1;
			id[1]++;
			id[2] -= ex_cover->size - 1;
		} else if (i != 0 && i % ex_cover->size == 0) {
			/* Moving to the next column in the Sudoku */
			id[0] -= ex_cover->size - 1;
			id[2]++;
		} else {
			/* Next value for the same cell */
			id[0]++;
		}

		/* Create nodes for each 1 in the current row of the matrix */
		for (j = 0; j < ex_cover->col; j++, cur = cur->right) {
			if (ex_cover->matrix[i][j]) {
				struct node *node = malloc(sizeof(struct node));

				/* Track the node for memory management */
				ex_cover->deallocate[ex_cover->count++] = node;

				/* Store the Sudoku meaning of this node */
				node->id[0] = id[0];	/* Value */
				node->id[1] = id[1];	/* Row */
				node->id[2] = id[2];	/* Column */

				/* Special case for first node in a row */
				if (prev == NULL) {
					prev = node;
					prev->right = node;	/* Self-reference initially */
				}

				/* Link horizontally (within the row) */
				node->left = prev;
				node->right = prev->right;
				node->right->left = node;
				prev->right = node;

				/* Link vertically (within the column) */
				node->up = cur->up;
				node->down = cur;
				node->colHead = cur;	/* Point to column header */
				cur->up->down = node;
				(cur->size)++;		/* Increment column size */
				cur->up = node;		/* Update column's up pointer */

				/* If this is the first node in a column */
				if (cur->down == cur)
					cur->down = node;

				prev = node;	/* Update previous node reference */
			}
		}
	}

	/* Store the head of the structure */
	ex_cover->head = head;
}

/*
 * Function for transforming the Toroidal Double Linked List based on the Sudoku Grid.
 * Pre-processes the Dancing Links structure to account for the initial puzzle state.
 */
void transformTorodialDList(struct ExactCover *ex_cover, struct Sudoku *sudoku)
{
	int i, j;	/* Iterators for grid rows and columns */
	int pos;	/* Position counter for original array */

	pos = 0;

	/* Process each cell in the Sudoku grid */
	for (i = 0; i < sudoku->size; i++) {
		for (j = 0; j < sudoku->size; j++) {
			/* For cells that already have a value */
			if (sudoku->grid[i][j] != 0) {
				struct node *col;	/* Column pointer */
				struct node *temp;	/* Node pointer */
				int flag;		/* Found flag */

				col = NULL;
				temp = NULL;
				flag = 0;

				/* Find the node matching this pre-filled cell */
				for (col = ex_cover->head->right;
				     col != ex_cover->head; col = col->right) {
					for (temp = col->down; temp != col;
					     temp = temp->down) {
						/* Check if this node matches the grid value and position */
						if (temp->id[0] ==
							    sudoku->grid[i][j] &&
						    temp->id[1] - 1 == i &&
						    temp->id[2] - 1 == j) {
							flag = 1;
							break;
						}
					}
					if (flag)
						break;
				}

				/* If matching node found, apply the choice */
				if (flag) {
					struct node *node;

					/* Cover this column */
					cover(col);

					/* Store in original array to restore later */
					ex_cover->original[pos++] = temp;

					/* Cover all other columns affected by this choice */
					for (node = temp->right; node != temp;
					     node = node->right)
						cover(node->colHead);
				}
			}
		}
	}
}

/*
 * Function for mapping the answer of the Exact Cover Problem to the Sudoku Grid.
 * Takes the solution found by Algorithm X and fills the Sudoku grid.
 */
void MapAnswer(struct ExactCover *ex_cover, struct Sudoku *sudoku)
{
	int i;

	/* Map the found solution (rows from the dancing links) */
	for (i = 0; ex_cover->answer[i] != NULL; i++)
		sudoku->grid[ex_cover->answer[i]->id[1] - 1]
			    [ex_cover->answer[i]->id[2] - 1] =
			ex_cover->answer[i]->id[0];

	/* Map the original values (pre-filled cells) */
	for (i = 0; ex_cover->original[i] != NULL; i++)
		sudoku->grid[ex_cover->original[i]->id[1] - 1]
			    [ex_cover->original[i]->id[2] - 1] =
			ex_cover->original[i]->id[0];
}

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
