#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../include/debug.h"
#include "../../include/linked_list.h"
#include "../../include/solver.h"
#include "../../include/solver_parallel.h"
#include "../../include/sudoku_utils.h"

/* Helper function: Checks if a value exists in a linked list. */
int has_value(struct node *head, int value) {
	struct node *current; 
	current = head;
	while (current != NULL) {
		if (current->data == value) {
			return 1; /* Value found */
		}
		current = current->next;
	}
	return 0; /* Value not found */
}

int is_coord_in_tuple(int r, int c, struct coordinates *coord, int n_coordinates) {
	int i; 
	for (i = 0; i < n_coordinates; ++i) {
		if (coord[i].row == r && coord[i].column == c) {
			return 1;
		}
	}
	return 0;
}

int parallel_propagate_row(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate) {
	int changes;
	int j;
	int target_row;
	struct node* original_head;
	int original_size;

	changes = 0; /* Initialize count of changes made */
	target_row = coord[0].row;

	for (j = 0; j < n; ++j) {
		if (!is_coord_in_tuple(target_row, j, coord, n_coordinates)) {
			original_head = extended_grid[target_row][j];
			original_size = size_list(original_head);

			extended_grid[target_row][j] = delete_at_given_value(original_head, value_to_propagate);
			
			if (size_list(extended_grid[target_row][j]) < original_size) {
				changes++;
			}
		}
	}
	return changes;
}

int parallel_propagate_column(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate) {
	int changes; 
	int i;       
	int target_col;
	struct node* original_head;
	int original_size;

	changes = 0;
	target_col = coord[0].column;

	for (i = 0; i < n; ++i) {
		if (!is_coord_in_tuple(i, target_col, coord, n_coordinates)) {
			original_head = extended_grid[i][target_col];
			original_size = size_list(original_head);

			extended_grid[i][target_col] = delete_at_given_value(original_head, value_to_propagate);
			
			if (size_list(extended_grid[i][target_col]) < original_size) {
				changes++;
			}
		}
	}
	return changes;
}


int parallel_propagate_box(struct node ***extended_grid, int n, struct coordinates *coord, int n_coordinates, int value_to_propagate) {
	int changes;
	int r, c;
	int sqrt_n;
	int box_start_row;
	int box_start_col;
	struct node* original_head;
	int original_size;

	changes = 0;
	sqrt_n = (int)sqrt(n);
	box_start_row = (coord[0].row / sqrt_n) * sqrt_n;
	box_start_col = (coord[0].column / sqrt_n) * sqrt_n;

	for (r = box_start_row; r < box_start_row + sqrt_n; ++r) {
		for (c = box_start_col; c < box_start_col + sqrt_n; ++c) {
			if (!is_coord_in_tuple(r, c, coord, n_coordinates)) {
				original_head = extended_grid[r][c];
				original_size = size_list(original_head);

				extended_grid[r][c] = delete_at_given_value(original_head, value_to_propagate);
				
				if (size_list(extended_grid[r][c]) < original_size) {
					changes++;
				}
			}
		}
	}
	return changes;
}


int parallel_hidden_singles(struct node ***extended_grid, int n) {
	int total_changes;
	int value_to_find;
	int r, c;
	int found_count;
	int last_found_r, last_found_c;
	int sqrt_n;
	int box_start_row, box_start_col;
	int box_r, box_c; /* For iterating within a box */
	struct node* current_cell_candidates; /* Temporary pointer for cell's candidates */
	int original_size_of_cell;

	total_changes = 0;
	sqrt_n = (int)sqrt(n);

	DPRINTF("\nApplying Hidden Singles technique...\n");

	for (value_to_find = 1; value_to_find <= n; ++value_to_find) {

		/* --- Check for Hidden Singles in Rows --- */
		for (r = 0; r < n; ++r) {
			found_count = 0;
			last_found_r = -1;
			last_found_c = -1;
			for (c = 0; c < n; ++c) {
				current_cell_candidates = extended_grid[r][c];
				if (current_cell_candidates != NULL && has_value(current_cell_candidates, value_to_find)) {
					found_count++;
					last_found_r = r;
					last_found_c = c;
				}
			}

			if (found_count == 1) {
				if (size_list(extended_grid[last_found_r][last_found_c]) > 1) {
					DPRINTF("Hidden Single (Row): Value %d found only in [%d][%d]. Eliminating other candidates.\n",
						value_to_find, last_found_r + 1, last_found_c + 1);
					
					original_size_of_cell = size_list(extended_grid[last_found_r][last_found_c]);
					free_list(extended_grid[last_found_r][last_found_c]);
					extended_grid[last_found_r][last_found_c] = create_node(value_to_find);
					
					total_changes += (original_size_of_cell - 1);
				}
			}
		}

		/* --- Check for Hidden Singles in Columns --- */
		for (c = 0; c < n; ++c) {
			found_count = 0;
			last_found_r = -1;
			last_found_c = -1;

			for (r = 0; r < n; ++r) {
				current_cell_candidates = extended_grid[r][c];
				if (current_cell_candidates != NULL && has_value(current_cell_candidates, value_to_find)) {
				found_count++;
				last_found_r = r;
				last_found_c = c;
				}
			}

			if (found_count == 1) {
				if (size_list(extended_grid[last_found_r][last_found_c]) > 1) {
					DPRINTF("Hidden Single (Column): Value %d found only in [%d][%d]. Eliminating other candidates.\n",
						value_to_find, last_found_r + 1, last_found_c + 1);
					
					original_size_of_cell = size_list(extended_grid[last_found_r][last_found_c]);
					free_list(extended_grid[last_found_r][last_found_c]);
					extended_grid[last_found_r][last_found_c] = create_node(value_to_find);
					
					total_changes += (original_size_of_cell - 1);
				}
			}
		}

		/* --- Check for Hidden Singles in Boxes --- */
		for (box_r = 0; box_r < sqrt_n; ++box_r) {
			for (box_c = 0; box_c < sqrt_n; ++box_c) {
				found_count = 0;
				last_found_r = -1;
				last_found_c = -1;

				box_start_row = box_r * sqrt_n;
				box_start_col = box_c * sqrt_n;

				for (r = box_start_row; r < box_start_row + sqrt_n; ++r) {
					for (c = box_start_col; c < box_start_col + sqrt_n; ++c) {
						current_cell_candidates = extended_grid[r][c];
						if (current_cell_candidates != NULL && has_value(current_cell_candidates, value_to_find)) {
							found_count++;
							last_found_r = r;
							last_found_c = c;
						}
					}
				}

				if (found_count == 1) {
					if (size_list(extended_grid[last_found_r][last_found_c]) > 1) {
						DPRINTF("Hidden Single (Box): Value %d found only in [%d][%d]. Eliminating other candidates.\n",
							value_to_find, last_found_r + 1, last_found_c + 1);
						
						original_size_of_cell = size_list(extended_grid[last_found_r][last_found_c]);
						free_list(extended_grid[last_found_r][last_found_c]);
						extended_grid[last_found_r][last_found_c] = create_node(value_to_find);
						
						total_changes += (original_size_of_cell - 1);
					}
				}
			}
		}
	}
	return total_changes;
}

int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int eliminations;
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

	eliminations = 0; /* Set eliminations to 0, since nothing changed yet */

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
						eliminations += parallel_propagate_row(
							extended_grid, n, coord, depth,
							temp2->data); /* Pass 'depth' as n_coordinates */
						temp2 = temp2->next;
					}
	
					/* Mark involved cells as propagated */
					for (l = 0; l < depth; ++l) {
						already_propagated[coord[l].row][coord[l].column] = 1;
					}	
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

	return eliminations;
}

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col)
{
	int i, j; /* Loop variables to go through the matrix */
	int k; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int eliminations;
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

	eliminations = 0; /* Set eliminations to 0, since nothing changed yet */

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
						eliminations += parallel_propagate_column(
							extended_grid, n, coord, depth,
							temp2->data); /* Pass 'depth' as n_coordinates */
						temp2 = temp2->next;
					}
	
					/* Mark involved cells as propagated */
					for (l = 0; l < depth; ++l) {
						already_propagated[coord[l].row][coord[l].column] = 1;
					}
	
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

	return eliminations;
}

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row)
{
	int i, j; /* Loop variables to go through the matrix */
	int k, m; /* Temp loop variable to continue to search for matches */
	int l; /* Loop variable to save the coordinates */
	int remaining_nodes;
	int eliminations;
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
	eliminations = 0; /* Set eliminations to 0, since nothing changed yet */

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
								eliminations += parallel_propagate_box(
									extended_grid, n, coord, depth,
									temp2->data); /* Pass 'depth' as n_coordinates */
								temp2 = temp2->next;
							}
			
							/* Mark involved cells as propagated */
							for (l = 0; l < depth; ++l) {
								already_propagated[coord[l].row][coord[l].column] = 1;
							}
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

	return eliminations;
}

