
/* SPDX-License-Identifier: GPL-3.0 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../../include/debug.h"
#include "../../include/solver_v2.h"
#include "../../include/bitarray.h"
#define DEBUG
// DONE
int sudoku_solver(int **grid, int n)
{
    int i, j; /* Loop variables */
	int is_changed; /* Flag to check if any changes are made */
	int depth;
	int max_depth;
    struct board *b;

    int ***already_propagated_rows;
	int ***already_propagated_columns;
	int ***already_propagated_boxes;
	int **selected_propagated;

        /* TODO: Add checks for errors */
	max_depth = (int)floor((double)n / 2);
	DPRINTF("Max depth: %d\n\n", max_depth);
	already_propagated_rows = (int ***)malloc(max_depth * sizeof(int **));
	for (i = 0; i < max_depth; ++i) {
		already_propagated_rows[i] = (int **)malloc(n * sizeof(int *));
		for (j = 0; j < n; ++j)
			already_propagated_rows[i][j] = (int *)malloc(n * sizeof(int));
		
		initialize_propagation_matrix(already_propagated_rows[i], n);
	}

	already_propagated_columns = (int ***)malloc(max_depth * sizeof(int **));
	for (i = 0; i < max_depth; ++i) {
		already_propagated_columns[i] = (int **)malloc(n * sizeof(int *));
		for (j = 0; j < n; ++j)
			already_propagated_columns[i][j] = (int *)malloc(n * sizeof(int));
		
		initialize_propagation_matrix(already_propagated_columns[i], n);
	}

	already_propagated_boxes = (int ***)malloc(max_depth * sizeof(int **));
	for (i = 0; i < max_depth; ++i) {
		already_propagated_boxes[i] = (int **)malloc(n * sizeof(int *));
		for (j = 0; j < n; ++j)
			already_propagated_boxes[i][j] = (int *)malloc(n * sizeof(int));
		
		initialize_propagation_matrix(already_propagated_boxes[i], n);
	}

	/* Create an extended grid */
	b = init_board(n);
        populate_board(b, grid);
        DPRINT_BOARD(b);

	/* Solve the Sudoku puzzle using constraint propagation */
	do {
		is_changed = 0; /* Reset the flag for each iteration */

		/* Use the technique of naked candidates */
		for (depth = 1; depth <= max_depth; ++depth) {
			selected_propagated = already_propagated_rows[depth - 1];
			is_changed += naked_candidates_rows(b,
				n, selected_propagated, depth);
			
			DPRINTF("\n\nPropagation at depth (row): %d\n", depth);
			DPRINT_BOARD(b);
			DPRINTF("\n\n\n");

			selected_propagated = already_propagated_columns[depth - 1];
			is_changed += naked_candidates_columns(b,
				n, selected_propagated, depth);
		
			DPRINTF("\n\nPropagation at depth (col): %d\n", depth);
			DPRINT_BOARD(b);
			DPRINTF("\n\n\n");
			
			selected_propagated = already_propagated_boxes[depth - 1];
			is_changed += naked_candidates_boxes(b,
				n, selected_propagated, depth);
			
			DPRINTF("\n\nPropagation at depth (box): %d\n", depth);
			DPRINT_BOARD(b);
			DPRINTF("\n\n\n");
		}

		DPRINTF("\nUpdated board:\n");
		DPRINT_BOARD(b);
		DPRINTF("\n\n\n");
	} while (is_changed);

	/* Convert in the original grid */
	board_to_grid(b, grid);

	/* Free everything */
        free_board(b);
	free_propagation_matrix(already_propagated_rows, n);
	free_propagation_matrix(already_propagated_columns, n);
	free_propagation_matrix(already_propagated_boxes, n);

	return 0;
}


// DONE
struct board *init_board(int n)
{
	int i, j;
	struct board *b;

    //const int MULTIPOTENT_CANDIDATES = 1 << (n - 1) | ((1<< (n - 1)) - 1 );
    b = (struct board *) malloc(sizeof(struct board));
    b->n = n;
    b->cells = (struct cell **) malloc(n * sizeof(struct cell*));

	for (i = 0; i < n; ++i) {
		b->cells[i] = (struct cell *) malloc(n * sizeof(struct cell));
		for (j = 0; j < n; ++j)
        {
			b->cells[i][j].candidates = 0;
            b->cells[i][j].has_value = 0;
            b->cells[i][j].value = 0;
        }
	}

	return b;
}

// DONE
void populate_board(struct board *b, int **grid)
{
	int i, j, k;
	int n = b->n;

	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			if (grid[i][j] != 0) {
				b->cells[i][j].has_value = 1;
                b->cells[i][j].value = grid[i][j];
                b->cells[i][j].candidates = set(0, grid[i][j]);
			} 
		}
	}
}
// DONE
void print_board(const struct board *b)
{
	int i, j, k;
	int n = b->n;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
            printf("%d", b->cells[i][j].value);

			if (j < n - 1)
				printf(" ");
		}
		printf("\n");
	}
}

// DONE
void board_to_grid(const struct board *b, int **grid)
{
	int i, j, k;
	int n = b->n;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (is_one_bit(b->cells[i][j].candidates)) {
				/* Exactly one candidate: find which number */
                b->cells[i][j].value=outer_bit_pos(b->cells[i][j].candidates);
                b->cells[i][j].has_value = 1;
			} else {
				/* Multiple candidates or none: unsolved */
				grid[i][j] = 0;
			}
		}
	}
}

// DONE
void free_board(struct board *b)
{
	int i;

	for (i = 0; i < b->n; i++)
		free(b->cells[i]);

	free(b->cells);
	free(b);
}

// DONE
void initialize_propagation_matrix(int **matrix, int n)
{
	int i, j;

	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j)
			matrix[i][j] = 0;
}

// DONE
void free_propagation_matrix(int ***propagation, int n)
{
	int i, j;
	int max_depth;

	max_depth = (int)floor((double)n / 2);
	for (i = 0; i < max_depth; ++i) {
		for (j = 0; j < n; ++j)
			free(propagation[i][j]);
		free(propagation[i]);
	}
	free(propagation);
}

// DONE
int naked_candidates_rows(struct board *b, int n,
			  int **already_propagated, int depth)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int changed;
	int n_difference;
	struct cell cell;
	int candidates; /* Bitarray for the union of candidates in the tuple */
	struct coordinates *coord;
    int n_candidates;

	DPRINTF("\nElimination of naked candidates (row) at depth %d\n", depth);

	/* Set coordinates array to length depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	changed = 0; /* Set changed to 0, since nothing changed yet */

	/* Explore board row-wise */
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			cell = b->cells[i][j];
            n_candidates = popcount(cell.candidates);
            

			DPRINTF("\tAt cell [%d][%d]: popcount=%d\n",
				i + 1, j + 1, popcount(cell.candidates));

			/* Skip empty cells (no candidates) */
			if (n_candidates == 0) {
				DPRINTF("\t\t - No values in this cell\n");
				continue;
			}

			/* Check the right depth */
			if (n_candidates > depth) {
				DPRINTF("\t\t - More than %d values in this cell\n",
					depth);
				continue;
			}

			/* Exclude naked singles for superior tuples */
			if (n_candidates == 1 && depth > 1)
				continue;

			/* If we are in this section of the code it means we
			 * found something with a good depth */
			DPRINTF("\t\tRight number of values\n");

			/* Check if already propagated */
			if (already_propagated[i][j]) {
				DPRINTF("\t - Cell [%d][%d] already propagated\n",
					i + 1, j + 1);
				continue;
			}

			/* Start building the tuple */
			remaining_nodes = depth;
			candidates = cell.candidates;
			l = 0;

			/* Save node coordinates */
			coord[l].row = i;
			coord[l].column = j;
			l++;

			/* Subtract from counter to signal the possible candidate */
			DPRINTF("Remaining nodes: %d", remaining_nodes);
			--remaining_nodes;
			DPRINTF("...%d\n", remaining_nodes);

			/* If needed for the tuple, search for other candidates
			 * on the row */
			for (k = j + 1; k < n && remaining_nodes != 0; ++k) {
				/* Exclude adding singles to the tuple */
				if (popcount(b->cells[i][k].candidates) <= 1)
					continue;

				/* Count how many new candidates this cell adds */
				n_difference = popcount(b->cells[i][k].candidates & ~candidates);

				DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n",
					i + 1, k + 1, n_difference);

				if ((n_candidates + n_difference) <= depth) {
					/* Add new values to candidates */
					DPRINTF("\t\t\tAdding new candidates\n");
					candidates |= b->cells[i][k].candidates;

					coord[l].row = i;
					coord[l].column = k;
					l++;

					--remaining_nodes;
					if (remaining_nodes == 0)
						break;
				}
			}

			if (remaining_nodes == 0) {
				/* Found a complete naked tuple of size 'depth' */
				DPRINTF("\nFound naked tuple of size %d at cells: ",
					depth);
				for (l = 0; l < depth; ++l) {
					DPRINTF("[%d][%d] ",
						coord[l].row + 1,
						coord[l].column + 1);
				}
				DPRINTF("\n");

				/* Propagate each value in the candidates bitarray */
				for (k = 1; k <= n; ++k) {
					if (test_bit(candidates, k)) {
						propagate_row(b, n, coord,
							depth, k);
					}
				}

				/* Mark involved cells as propagated */
				for (l = 0; l < depth; ++l) {
					already_propagated[coord[l].row]
						[coord[l].column] = 1;
				}
				changed = 1;

				DPRINTF("\nPropagation complete.\n\n");
			} else {
				DPRINTF("\t\tDid not find enough matching cells"
					" for a tuple starting at [%d][%d]\n\n",
					i + 1, j + 1);
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}
//
// DONE
int naked_candidates_columns(struct board *b, int n,
			  int **already_propagated, int depth)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int changed;
	int n_difference;
	struct cell cell;
	int candidates;
    int n_candidates;
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (col) at depth %d\n", depth);

	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	changed = 0;

	/* Explore board column-wise */
	for (j = 0; j < n; ++j) {
		for (i = 0; i < n; ++i) {
			cell = b->cells[i][j];
            n_candidates = popcount(cell.candidates);

			if (cell.candidates == 0)
				continue;
			if (n_candidates > depth)
				continue;
			if (n_candidates == 1 && depth > 1)
				continue;
			if (already_propagated[i][j])
				continue;

			remaining_nodes = depth;
			candidates = cell.candidates;
			l = 0;

			coord[l].row = i;
			coord[l].column = j;
			l++;
			--remaining_nodes;

			/* Search for other candidates in the column */
			for (k = i + 1; k < n && remaining_nodes != 0; ++k) {
				if (popcount(b->cells[k][j].candidates) <= 1)
					continue;

				n_difference = popcount(b->cells[k][j].candidates & ~candidates);

				if ((popcount(candidates) + n_difference) <= depth) {
					candidates |= b->cells[k][j].candidates;

					coord[l].row = k;
					coord[l].column = j;
					l++;

					--remaining_nodes;
					if (remaining_nodes == 0)
						break;
				}
			}

			if (remaining_nodes == 0) {
				DPRINTF("\nFound naked tuple (col) of size %d at cells: ",
					depth);
				for (l = 0; l < depth; ++l)
					DPRINTF("[%d][%d] ",
						coord[l].row + 1,
						coord[l].column + 1);
				DPRINTF("\n");

				for (k = 1; k <= n; ++k) {
					if (test_bit(candidates, k))
						propagate_column(b, n, coord,
							depth, k);
				}

				for (l = 0; l < depth; ++l)
					already_propagated[coord[l].row]
						[coord[l].column] = 1;
				changed = 1;

				DPRINTF("\nPropagation complete.\n\n");
			}
		}
	}

	free(coord);
	return changed;
}

// DONE
int naked_candidates_boxes(struct board *b, int n,
			  int **already_propagated, int depth)
{
	int bi, bj; /* Box indices */
	int ci, cj; /* Cell indices within a box */
	int ki, kj; /* Search indices within the box */
	int k, l;
	int box_size;
	int remaining_nodes;
	int changed;
	int n_difference;
	struct cell  cell;
	int candidates;
    int n_candidates;
	int row, col, krow, kcol;
	int start; /* Flag: found the starting cell in the scan */
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (box) at depth %d\n", depth);

	box_size = (int)sqrt(n);

	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	changed = 0;

	/* Iterate over each box */
	for (bi = 0; bi < box_size; ++bi) {
		for (bj = 0; bj < box_size; ++bj) {
			/* Iterate over cells within the box */
			for (ci = 0; ci < box_size; ++ci) {
				for (cj = 0; cj < box_size; ++cj) {
					row = bi * box_size + ci;
					col = bj * box_size + cj;
					cell = b->cells[row][col];
                    n_candidates = popcount(cell.candidates);

					if (cell.candidates == 0)
						continue;
					if (n_candidates > depth)
						continue;
					if (n_candidates == 1 && depth > 1)
						continue;
					if (already_propagated[row][col])
						continue;

					remaining_nodes = depth;
					candidates = cell.candidates;
					l = 0;

					coord[l].row = row;
					coord[l].column = col;
					l++;
					--remaining_nodes;

					/* Search remaining cells in the box
					 * (after the current one) */
					start = 0;
					for (ki = 0; ki < box_size &&
					     remaining_nodes > 0; ++ki) {
						for (kj = 0; kj < box_size &&
						     remaining_nodes > 0; ++kj) {
							/* Skip cells up to and
							 * including (ci, cj) */
							if (!start) {
								if (ki == ci &&
								    kj == cj)
									start = 1;
								continue;
							}

							krow = bi * box_size + ki;
							kcol = bj * box_size + kj;

							if (popcount(b->cells[krow][kcol].candidates) <= 1)
								continue;

							n_difference = popcount(
								b->cells[krow][kcol].candidates
								& ~candidates);

							if ((popcount(candidates) +
							     n_difference) <= depth) {
								candidates |=
									b->cells[krow][kcol].candidates;

								coord[l].row = krow;
								coord[l].column = kcol;
								l++;

								--remaining_nodes;
							}
						}
					}

					if (remaining_nodes == 0) {
						DPRINTF("\nFound naked tuple (box)"
							" of size %d at cells: ",
							depth);
						for (l = 0; l < depth; ++l)
							DPRINTF("[%d][%d] ",
								coord[l].row + 1,
								coord[l].column + 1);
						DPRINTF("\n");

						for (k = 1; k <= n; ++k) {
							if (test_bit(candidates, k))
								propagate_box(b, n,
									coord,
									depth, k);
						}

						for (l = 0; l < depth; ++l)
							already_propagated
								[coord[l].row]
								[coord[l].column] = 1;
						changed = 1;

						DPRINTF("\nPropagation complete.\n\n");
					}
				}
			}
		}
	}

	free(coord);
	return changed;
}

// DONE
void propagate_row(struct board *b, int n, struct coordinates *coord,
		   int n_coordinates, int value)
{
	int i, j;
	int row = coord[0].row;
	int skip;
	int mask = ~(1 << value);

	DPRINTF("\nPropagating value %d on row %d\n", value, row + 1);

	for (i = 0; i < n; ++i) {
		skip = 0;
		for (j = 0; j < n_coordinates; ++j) {
			if (i == coord[j].column) {
				skip = 1;
				break;
			}
		}

		if (!skip)
			b->cells[row][i].candidates &= mask;
	}
}

// DONE
void propagate_column(struct board *b, int n, struct coordinates *coord,
		      int n_coordinates, int value)
{
	int i, j;
	int col = coord[0].column;
	int skip;
	int mask = ~(1 << value);

	DPRINTF("\nPropagating value %d on column %d\n", value, col + 1);

	for (i = 0; i < n; ++i) {
		skip = 0;
		for (j = 0; j < n_coordinates; ++j) {
			if (i == coord[j].row) {
				skip = 1;
				break;
			}
		}

		if (!skip)
			b->cells[i][col].candidates &= mask;
	}
}

// DONE
void propagate_box(struct board *b, int n, struct coordinates *coord,
		   int n_coordinates, int value)
{
	int i, j, k;
	int box_size = (int)sqrt(n);
	int box_row = (coord[0].row / box_size) * box_size;
	int box_col = (coord[0].column / box_size) * box_size;
	int skip;
	int mask = ~(1 << value);

	DPRINTF("\nPropagating value %d on box [%d,%d]\n",
		value, box_row / box_size + 1, box_col / box_size + 1);

	for (i = box_row; i < box_row + box_size; ++i) {
		for (j = box_col; j < box_col + box_size; ++j) {
			skip = 0;
			for (k = 0; k < n_coordinates; ++k) {
				if (i == coord[k].row &&
				    j == coord[k].column) {
					skip = 1;
					break;
				}
			}

			if (!skip)
				b->cells[i][j].candidates &= mask;
		}
	}
}
