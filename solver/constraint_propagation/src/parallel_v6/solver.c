#define _POSIX_C_SOURCE 200112L

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/debug.h"
#include "../../include/linked_list.h"
#include "../../include/solver.h"
#include "../../include/solver_parallel2.h"
#include "../../include/sudoku_utils.h"

/**
 * Parallel sudoku solver using pthreads.
 *
 * Each thread in a threadgroup calls this function with the same shared
 * extended_grid. Threads work on non-overlapping row/column ranges and
 * synchronize via barriers between propagation phases to avoid races.
 *
 * @param extended_grid Shared extended grid (already created by thread 1)
 * @param n             Sudoku size
 * @param thread_id     1-based thread ID within the threadgroup
 * @param tg_size       Number of threads in the threadgroup
 * @param barrier       Barrier initialized to tg_size for synchronization
 * @param shared_is_changed Shared variable for reducing is_changed across threads
 * @return 0 on success, -1 on error
 */
int parallel_sudoku_solver(struct node ***extended_grid, int n,
			   int thread_id, int tg_size,
			   pthread_barrier_t *barrier,
			   volatile int *shared_is_changed)
{
	int i, j; /* Loop variables */
	int depth;
	int max_depth;
	int numbers_left;
	int local_is_changed;
	int should_continue;

	int ***already_propagated_rows;
	int ***already_propagated_columns;
	int ***already_propagated_boxes;
	int **selected_propagated;

	int sqrt_n;
	int start, end;

	sqrt_n = (int)sqrt(n);
	start = ((thread_id - 1) % sqrt_n) * sqrt_n;
	end = start + sqrt_n;

	DPRINTF("Thread %d/%d: Starting row/col: %d. Ending row/col: %d\n\n",
		thread_id, tg_size, start, end - 1);

	/* Allocate per-thread propagation matrices */
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

	/* Print the extended grid (only thread 1) */
	if (thread_id == 1) {
		DPRINTF("\nExtended grid:\n");
		DPRINT_EXTENDED_GRID(extended_grid, n);
	}
	pthread_barrier_wait(barrier);

	/* Solve the Sudoku puzzle using constraint propagation */
	do {
		local_is_changed = 0;

		/* Use the technique of naked candidates */
		for (depth = 1; depth <= max_depth; ++depth) {
			/* Row propagation - threads work on disjoint row ranges */
			selected_propagated = already_propagated_rows[depth - 1];
			local_is_changed += parallel_naked_candidates_rows(
				extended_grid, n, selected_propagated, depth,
				start, end);

			DPRINTF("\n\nPropagation at depth (row): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");

			/* Barrier: all threads done with row propagation */
			pthread_barrier_wait(barrier);

			/* Column propagation - threads work on disjoint col ranges */
			selected_propagated = already_propagated_columns[depth - 1];
			local_is_changed += parallel_naked_candidates_cols(
				extended_grid, n, selected_propagated, depth,
				start, end);

			DPRINTF("\n\nPropagation at depth (col): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");

			/* Barrier: all threads done with column propagation */
			pthread_barrier_wait(barrier);

			/* Box propagation - threads work on disjoint box ranges */
			selected_propagated = already_propagated_boxes[depth - 1];
			local_is_changed += parallel_naked_candidates_boxes(
				extended_grid, n, selected_propagated, depth,
				start, end);

			DPRINTF("\n\nPropagation at depth (box): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");

			/* Barrier: all threads done with box propagation */
			pthread_barrier_wait(barrier);
		}

		/* Reduce is_changed across all threads in the group */
		__sync_fetch_and_add(shared_is_changed, local_is_changed);
		pthread_barrier_wait(barrier);

		should_continue = (*shared_is_changed != 0);

		/* Barrier before reset so all threads have read the value */
		pthread_barrier_wait(barrier);
		if (thread_id == 1)
			*shared_is_changed = 0;
		pthread_barrier_wait(barrier);

	} while (should_continue);

	/* Count numbers left for progress (thread 1 only) */
	if (thread_id == 1) {
		numbers_left = 0;
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++) {
				struct node *temp = extended_grid[i][j];
				while (temp != NULL) {
					numbers_left++;
					temp = temp->next;
				}
			}
		}
		DPRINTF("Numbers left in the extended grid: %d\n", numbers_left);
		DPRINTF("Progress: %2.1f%%\n",
			(double)((double)1 - (double)(numbers_left - n * n) /
				(double)((n * n * n) - (n * n))) * 100);
	}

	/* Free per-thread propagation matrices */
	free_propagation_matrix(already_propagated_rows, n);
	free_propagation_matrix(already_propagated_columns, n);
	free_propagation_matrix(already_propagated_boxes, n);

	return 0;
}


int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (row) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	changed = 0; /* Set changed to 0, since nothing changed yet */

	/* Explore extended grid row-wise */
	for (i = start_row; i < end_row; ++i) {
		for (j = 0; j < n; ++j) {
			remaining_nodes = depth;
			candidates = NULL; /* Reset candidates list for each potential starting node */
			l = 0; /* Reset coordinate index for each potential starting node 'i' */

			temp = extended_grid[i][j];
			DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
			DPRINT_LIST(temp);
			DPRINTF("\n");

			if (temp == NULL) {
				DPRINTF("\t\t - No values in this cell\n");
				continue;
			}

			/* Check the right depth */
			if (size_list(temp) > depth) {
				DPRINTF("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			/* Exclude naked singles for superior tuples */
			if (size_list(temp) == 1 && depth > 1)
				continue;

			/* If we are in this section of the code it means we found something with a good depth */
			DPRINTF("\t\tRight number of values\n");

			/* Check if already propagated */
			if (!already_propagated[i][j]) {
				/* Append values in candidate list */
				temp2 = temp;
				do {
					candidates = append(candidates, temp2->data);
					if (candidates == NULL && temp2->data != 0) {
						fprintf(stderr, "Failed to append node in elimination\n");
						free(coord);
						return -1;
					}
					temp2 = temp2->next;
				} while (temp2 != NULL);

				/* Save node coordinates */
				coord[l].row = i;
				coord[l].column = j;
				l++;

				/* Subtract from counter to signal the possible candidate */
				DPRINTF("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				DPRINTF("...%d\n", remaining_nodes);

				/* If needed for the tuple, search for other candidates on the row */
				for (k = j + 1; k < n && remaining_nodes != 0; ++k) {
					/* Exclude adding singles to the tuple */
					if (size_list(extended_grid[i][k]) <= 1)
						continue;
	
					temp = extended_grid[i][k];
					n_difference = count_different_values(candidates, extended_grid[i][k]);
	
					DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", i + 1, k + 1, n_difference);
	
					if ((size_list(candidates) + n_difference) <= depth) {
						/* Add new values to candidates */
						DPRINTF("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[i][k]);
						DPRINT_LIST(candidates);
						DPRINTF("\n\n");
	
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
					DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l) {
						DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					}
					DPRINTF("\nValues to propagate: ");
					DPRINT_LIST(candidates);
					DPRINTF("\n");
	
					/* Propagate each value in the candidates list */
					temp2 = candidates;
					while (temp2 != NULL) {
						propagate_row(
							extended_grid, n, coord, depth,
							temp2->data); /* Pass 'depth' as n_coordinates */
						temp2 = temp2->next;
					}
	
					/* Mark involved cells as propagated */
					for (l = 0; l < depth; ++l) {
						already_propagated[coord[l].row][coord[l].column] = 1;
					}
					changed = 1; /* Signal that at least a change occurred */
	
					DPRINTF("\nPropagation complete.\n\n");
				} else {
					/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
					DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				/* Free the candidates list for the next iteration */
				free_list(candidates);
			} else {
				DPRINTF("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates); /* Free candidates if we skip due to already propagated */
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (column) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	changed = 0; /* Set changed to 0, since nothing changed yet */

	/* Explore extended grid column-wise */
	for (j = start_col; j < end_col; ++j) {
		for (i = 0; i < n; ++i) {
			remaining_nodes = depth;
			candidates = NULL; /* Reset candidates list for each potential starting node */
			l = 0; /* Reset coordinate index for each potential starting node 'i' */

			temp = extended_grid[i][j];
			DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
			DPRINT_LIST(temp);
			DPRINTF("\n");

			if (temp == NULL) {
				DPRINTF("\t\t - No values in this cell\n");
				continue;
			}

			/* Check the right depth */
			if (size_list(temp) > depth) {
				DPRINTF("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			/* Exclude naked singles for superior tuples */
			if (size_list(temp) == 1 && depth > 1)
				continue;

			/* If we are in this section of the code it means we found something with a good depth */
			DPRINTF("\t\tRight number of values\n");

			/* Check if already propagated */
			if (!already_propagated[i][j]) {
				/* Append values in candidate list */
				temp2 = temp;
				do {
					candidates = append(candidates, temp2->data);
					if (candidates == NULL && temp2->data != 0) {
						fprintf(stderr, "Failed to append node in elimination\n");
						free(coord);
						return -1;
					}
					temp2 = temp2->next;
				} while (temp2 != NULL);

				/* Save node coordinates */
				coord[l].row = i;
				coord[l].column = j;
				l++;

				/* Subtract from counter to signal the possible candidate */
				DPRINTF("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				DPRINTF("...%d\n", remaining_nodes);

				/* If needed for the tuple, search for other candidates on the column */
				for (k = i + 1; k < n && remaining_nodes != 0; ++k) {
					/* Exclude adding singles to the tuple */
					if (size_list(extended_grid[k][j]) <= 1)
						continue;
	
					temp = extended_grid[k][j];
					n_difference = count_different_values(candidates, extended_grid[k][j]);
	
					DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, j + 1, n_difference);
	
					if ((size_list(candidates) + n_difference) <= depth) {
						/* Add new values to candidates */
						DPRINTF("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[k][j]);
						DPRINT_LIST(candidates);
						DPRINTF("\n\n");
	
						coord[l].row = k;
						coord[l].column = j;
						l++;
	
						--remaining_nodes;
						if (remaining_nodes == 0)
							break;
					}
				}

				if (remaining_nodes == 0) {
					/* Found a complete naked tuple of size 'depth' */
					DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l) {
						DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					}
					DPRINTF("\nValues to propagate: ");
					DPRINT_LIST(candidates);
					DPRINTF("\n");
	
					/* Propagate each value in the candidates list */
					temp2 = candidates;
					while (temp2 != NULL) {
						propagate_column(
							extended_grid, n, coord, depth,
							temp2->data); /* Pass 'depth' as n_coordinates */
						temp2 = temp2->next;
					}
	
					/* Mark involved cells as propagated */
					for (l = 0; l < depth; ++l) {
						already_propagated[coord[l].row][coord[l].column] = 1;
					}
					changed = 1; /* Signal that at least a change occurred */
	
					DPRINTF("\nPropagation complete.\n\n");
				} else {
					/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
					DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				/* Free the candidates list for the next iteration */
				free_list(candidates);
			} else {
				DPRINTF("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates); /* Free candidates if we skip due to already propagated */
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row)
{
	int i, j; /* Loop variables to go through the matrix */
	int k, m; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	int sqrt_n;
	int box_row, box_col;
	int row_start, col_start;
	int given_row, given_col;

	DPRINTF("\nElimination of naked candidates (box) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	sqrt_n = (int)sqrt(n);
	changed = 0; /* Set changed to 0, since nothing changed yet */

	for (box_row = start_row / sqrt_n; box_row < end_row / sqrt_n; ++box_row) {
		for (box_col = 0; box_col < sqrt_n; ++box_col) {
			/* Calculate starting row and column for the current box */
			row_start = box_row * sqrt_n;
			col_start = box_col * sqrt_n;

			/* Iterate through the cells within the box */
			for (i = row_start; i < row_start + sqrt_n; ++i) {
                		for (j = col_start; j < col_start + sqrt_n; ++j) {
					remaining_nodes = depth;
					candidates = NULL; /* Reset candidates list for each potential starting node */
					l = 0; /* Reset coordinate index for each potential starting node 'i' */

					temp = extended_grid[i][j];
					DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
					DPRINT_LIST(temp);
					DPRINTF("\n");

					if (temp == NULL) {
						DPRINTF("\t\t - No values in this cell\n");
						continue;
					}

					/* Check the right depth */
					if (size_list(temp) > depth) {
						DPRINTF("\t\t - More than %d values in this cell\n",
						depth);
						continue;
					}

					/* Exclude naked singles for superior tuples */
					if (size_list(temp) == 1 && depth > 1)
						continue;

					/* If we are in this section of the code it means we found something with a good depth */
					DPRINTF("\t\tRight number of values\n");

					/* Check if already propagated */
					if (!already_propagated[i][j]) {
						/* Append values in candidate list */
						temp2 = temp;
						do {
							candidates = append(candidates, temp2->data);
							if (candidates == NULL && temp2->data != 0) {
								fprintf(stderr, "Failed to append node in elimination\n");
								free(coord);
								return -1;
							}
							temp2 = temp2->next;
						} while (temp2 != NULL);

						/* Save node coordinates */
						coord[l].row = i;
						coord[l].column = j;
						l++;

						/* Subtract from counter to signal the possible candidate */
						DPRINTF("Remaining nodes: %d", remaining_nodes);
						--remaining_nodes;
						DPRINTF("...%d\n", remaining_nodes);

						given_row = i;
						given_col = j;
						/* If needed for the tuple, search for other candidates on the box */
						for (k = row_start; k < row_start + sqrt_n && remaining_nodes != 0; ++k) {
							for (m = col_start; m < col_start + sqrt_n; ++m) {
								/* Condition to operate only on values after */
								if (k > given_row || (k == given_row && m > given_col)) {
									/* Exclude adding singles to the tuple */
									if (size_list(extended_grid[k][m]) <= 1)
										continue;
				
									temp = extended_grid[k][m];
									n_difference = count_different_values(candidates, extended_grid[k][m]);
					
									DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, m + 1, n_difference);
					
									if ((size_list(candidates) + n_difference) <= depth) {
										/* Add new values to candidates */
										DPRINTF("\t\t\tAdding new candidates to list...");
										candidates = add_new_candidates(candidates, extended_grid[k][m]);
										DPRINT_LIST(candidates);
										DPRINTF("\n\n");
					
										coord[l].row = k;
										coord[l].column = m;
										l++;
					
										--remaining_nodes;
										if (remaining_nodes == 0)
											break;
									}
								}

							}

							if (remaining_nodes == 0)
								break;
						}

						if (remaining_nodes == 0) {
							/* Found a complete naked tuple of size 'depth' */
							DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
							for (l = 0; l < depth; ++l) {
								DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
							}
							DPRINTF("\nValues to propagate: ");
							DPRINT_LIST(candidates);
							DPRINTF("\n");
			
							/* Propagate each value in the candidates list */
							temp2 = candidates;
							while (temp2 != NULL) {
								propagate_box(
									extended_grid, n, coord, depth,
									temp2->data); /* Pass 'depth' as n_coordinates */
								temp2 = temp2->next;
							}
			
							/* Mark involved cells as propagated */
							for (l = 0; l < depth; ++l) {
								already_propagated[coord[l].row][coord[l].column] = 1;
							}
							changed = 1; /* Signal that at least a change occurred */
			
							DPRINTF("\nPropagation complete.\n\n");
						} else {
							/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
							DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
							i + 1, j + 1);
						}

						/* Free the candidates list for the next iteration */
						free_list(candidates);
					} else {
						DPRINTF("\t - Cell [%d][%d] already propagated\n",
						i + 1, j + 1);
						free_list(candidates); /* Free candidates if we skip due to already propagated */
					}
				}
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}

/* communication() removed: with shared-memory pthreads the extended_grid
 * is directly shared among threads in a threadgroup, so no explicit
 * data exchange is needed. Barriers between propagation phases ensure
 * correctness. */
