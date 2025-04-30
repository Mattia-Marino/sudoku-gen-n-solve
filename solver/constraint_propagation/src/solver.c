#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/linked_list.h"
#include "../include/solver.h"
#include "../include/sudoku_utils.h"

int sudoku_solver(int **grid, int n)
{
	int i, j; /* Loop variables */
	int is_changed; /* Flag to check if any changes are made */
	int depth;
	int max_depth;
	int counter;
	int numbers_left;
	struct node ***extended_grid; /* Extended grid for constraint propagation */

	int ***already_propagated_rows;
	int ***already_propagated_columns;
	int ***already_propagated_boxes;
	int **selected_propagated;

	/* TODO: Add checks for errors */
	max_depth = (int)floor((double)n / 2);
	printf("Max depth: %d\n\n", max_depth);
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
	extended_grid = extend_grid(grid, n);
	if (extended_grid == NULL) {
		fprintf(stderr, "Error: Unable to create extended grid\n");
		return -1;
	}

	/* Print the extended grid */
	printf("\nExtended grid:\n");
	print_extended_grid(extended_grid, n);	

	/* Solve the Sudoku puzzle using constraint propagation */
	counter = 0;
	do {
		is_changed = 0; /* Reset the flag for each iteration */

		/* Use the technique of naked candidates */
		for (depth = 1; depth <= max_depth; ++depth) {
			selected_propagated = already_propagated_rows[depth - 1];
			is_changed += naked_candidates_rows(extended_grid,
				n, selected_propagated, depth);
			
			printf("\n\nPropagation at depth (row): %d\n", depth);
			print_extended_grid(extended_grid, n);
			printf("\n\n\n");

			selected_propagated = already_propagated_columns[depth - 1];
			is_changed += naked_candidates_columns(extended_grid,
				n, selected_propagated, depth);
		
			printf("\n\nPropagation at depth (col): %d\n", depth);
			print_extended_grid(extended_grid, n);
			printf("\n\n\n");
			
			selected_propagated = already_propagated_boxes[depth - 1];
			is_changed += naked_candidates_boxes(extended_grid,
				n, selected_propagated, depth);
			
			printf("\n\nPropagation at depth (box): %d\n", depth);
			print_extended_grid(extended_grid, n);
			printf("\n\n\n");
		}

		/* Use technique of hidden singles */
		printf("\n\nHidden singles...\n");
		is_changed += hidden_singles(extended_grid, n);

		/* Print the updated extended grid */
		printf("\nIteration %d - updated extended grid:\n", ++counter);
		print_extended_grid(extended_grid, n);
		printf("\n\n\n");
	} while (is_changed);

	/* Count numbers left for progress */
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
	printf("Numbers left in the extended grid: %d\n", numbers_left);
	printf("Progress: %2.1f%%\n",
	       (double)((double)1 - (double)(numbers_left - n * n) /
					    (double)((n * n * n) - (n * n))) *
		       100);
	printf("Total iterations: %d\n\n", counter);

	/* Fill the original grid with single values */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			struct node *temp = extended_grid[i][j];

			if (temp->next == NULL)
				grid[i][j] = temp->data;
		}
	}

	/* Free the extended grid */
	free_extended_grid(extended_grid, n);
	free_propagation_matrix(already_propagated_rows, n);
	free_propagation_matrix(already_propagated_columns, n);
	free_propagation_matrix(already_propagated_boxes, n);

	return 0;
}

struct node ***extend_grid(int **grid, int n)
{
	struct node ***extended_grid; /* Extended grid */
	int i, j, k; /* Loop variables */

	extended_grid = (struct node ***)malloc(n * sizeof(struct node **));
	if (extended_grid == NULL) {
		fprintf(stderr,
			"Error: Unable to allocate memory for extended grid\n");
		return NULL;
	}

	for (i = 0; i < n; i++) {
		extended_grid[i] =
			(struct node **)malloc(n * sizeof(struct node *));
		if (extended_grid[i] == NULL) {
			fprintf(stderr,
				"Error: Unable to allocate memory for extended grid row\n");
			for (j = 0; j < i; j++) {
				free(extended_grid[j]);
			}
			free(extended_grid);
			return NULL;
		}

		for (j = 0; j < n; j++) {
			extended_grid[i][j] = NULL;
			if (grid[i][j] != 0)
				extended_grid[i][j] =
					append(extended_grid[i][j], grid[i][j]);
			else
				for (k = 1; k <= n; k++)
					extended_grid[i][j] =
						append(extended_grid[i][j], k);

			if (extended_grid[i][j] == NULL) {
				fprintf(stderr,
					"Error: Unable to create or append node in extended grid\n");
				free_extended_grid(extended_grid, n);

				return NULL;
			}

			/* Debugging output */
			printf("Extended grid at [%d][%d]: ", i + 1, j + 1);
			print_list(extended_grid[i][j]);
			printf("\n");
		}
	}

	return extended_grid;
}

void initialize_propagation_matrix(int **matrix, int n)
{
	int i, j;

	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j)
			matrix[i][j] = 0;
}

int naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth)
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

	printf("\nElimination of naked candidates (row) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		printf("Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	changed = 0; /* Set changed to 0, since nothing changed yet */

	/* Explore extended grid row-wise */
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			remaining_nodes = depth;
			candidates = NULL; /* Reset candidates list for each potential starting node */
			l = 0; /* Reset coordinate index for each potential starting node 'i' */

			temp = extended_grid[i][j];
			printf("\tAt cell [%d][%d]: ", i + 1, j + 1);
			print_list(temp);
			printf("\n");

			if (temp == NULL) {
				printf("\t\t - No values in this cell\n");
				continue;
			}

			/* Check the right depth */
			if (size_list(temp) > depth) {
				printf("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			/* Exclude naked singles for superior tuples */
			if (size_list(temp) == 1 && depth > 1)
				continue;

			/* If we are in this section of the code it means we found something with a good depth */
			printf("\t\tRight number of values\n");

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
				printf("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				printf("...%d\n", remaining_nodes);

				/* If needed for the tuple, search for other candidates on the row */
				for (k = j + 1; k < n && remaining_nodes != 0; ++k) {
					/* Exclude adding singles to the tuple */
					if (size_list(extended_grid[i][k]) <= 1)
						continue;
	
					temp = extended_grid[i][k];
					n_difference = count_different_values(candidates, extended_grid[i][k]);
	
					printf("\t\t\tCell [%d][%d] - Difference: %d\n", i + 1, k + 1, n_difference);
	
					if ((size_list(candidates) + n_difference) <= depth) {
						/* Add new values to candidates */
						printf("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[i][k]);
						print_list(candidates);
						printf("\n\n");
	
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
					printf("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l) {
						printf("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					}
					printf("\nValues to propagate: ");
					print_list(candidates);
					printf("\n");
	
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
	
					printf("\nPropagation complete.\n\n");
				} else {
					/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
					printf("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				/* Free the candidates list for the next iteration */
				free_list(candidates);
			} else {
				printf("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates); /* Free candidates if we skip due to already propagated */
			}
		}
	}

	free(coord);
	printf("\n");

	return changed;
}

int naked_candidates_columns(struct node ***extended_grid, int n,
	int **already_propagated, int depth)
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

	printf("\nElimination of naked candidates (column) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		printf("Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	changed = 0; /* Set changed to 0, since nothing changed yet */

	/* Explore extended grid column-wise */
	for (j = 0; j < n; ++j) {
		for (i = 0; i < n; ++i) {
			remaining_nodes = depth;
			candidates = NULL; /* Reset candidates list for each potential starting node */
			l = 0; /* Reset coordinate index for each potential starting node 'i' */

			temp = extended_grid[i][j];
			printf("\tAt cell [%d][%d]: ", i + 1, j + 1);
			print_list(temp);
			printf("\n");

			if (temp == NULL) {
				printf("\t\t - No values in this cell\n");
				continue;
			}

			/* Check the right depth */
			if (size_list(temp) > depth) {
				printf("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			/* Exclude naked singles for superior tuples */
			if (size_list(temp) == 1 && depth > 1)
				continue;

			/* If we are in this section of the code it means we found something with a good depth */
			printf("\t\tRight number of values\n");

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
				printf("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				printf("...%d\n", remaining_nodes);

				/* If needed for the tuple, search for other candidates on the column */
				for (k = i + 1; k < n && remaining_nodes != 0; ++k) {
					/* Exclude adding singles to the tuple */
					if (size_list(extended_grid[k][j]) <= 1)
						continue;
	
					temp = extended_grid[k][j];
					n_difference = count_different_values(candidates, extended_grid[k][j]);
	
					printf("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, j + 1, n_difference);
	
					if ((size_list(candidates) + n_difference) <= depth) {
						/* Add new values to candidates */
						printf("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[k][j]);
						print_list(candidates);
						printf("\n\n");
	
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
					printf("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l) {
						printf("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					}
					printf("\nValues to propagate: ");
					print_list(candidates);
					printf("\n");
	
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
	
					printf("\nPropagation complete.\n\n");
				} else {
					/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
					printf("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				/* Free the candidates list for the next iteration */
				free_list(candidates);
			} else {
				printf("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates); /* Free candidates if we skip due to already propagated */
			}
		}
	}

	free(coord);
	printf("\n");

	return changed;
}

int naked_candidates_boxes(struct node ***extended_grid, int n,
	int **already_propagated, int depth)
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

	printf("\nElimination of naked candidates (box) at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		printf("Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	sqrt_n = (int)sqrt(n);
	changed = 0; /* Set changed to 0, since nothing changed yet */

	for (box_row = 0; box_row < sqrt_n; ++box_row) {
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
					printf("\tAt cell [%d][%d]: ", i + 1, j + 1);
					print_list(temp);
					printf("\n");

					if (temp == NULL) {
						printf("\t\t - No values in this cell\n");
						continue;
					}

					/* Check the right depth */
					if (size_list(temp) > depth) {
						printf("\t\t - More than %d values in this cell\n",
						depth);
						continue;
					}

					/* Exclude naked singles for superior tuples */
					if (size_list(temp) == 1 && depth > 1)
						continue;

					/* If we are in this section of the code it means we found something with a good depth */
					printf("\t\tRight number of values\n");

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
						printf("Remaining nodes: %d", remaining_nodes);
						--remaining_nodes;
						printf("...%d\n", remaining_nodes);

						given_row = i;
						given_col = j;
						/* If needed for the tuple, search for other candidates on the box */
						for (k = row_start; k < row_start + sqrt_n; ++k) {
							for (m = col_start; m < col_start + sqrt_n; ++m) {
								/* Condition to operate only on values after */
								if (k > given_row || (k == given_row && m > given_col)) {
									/* Exclude adding singles to the tuple */
									if (size_list(extended_grid[k][m]) <= 1)
										continue;
				
									temp = extended_grid[k][m];
									n_difference = count_different_values(candidates, extended_grid[k][m]);
					
									printf("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, m + 1, n_difference);
					
									if ((size_list(candidates) + n_difference) <= depth) {
										/* Add new values to candidates */
										printf("\t\t\tAdding new candidates to list...");
										candidates = add_new_candidates(candidates, extended_grid[k][m]);
										print_list(candidates);
										printf("\n\n");
					
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
							printf("\nFound naked tuple of size %d at cells: ", depth);
							for (l = 0; l < depth; ++l) {
								printf("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
							}
							printf("\nValues to propagate: ");
							print_list(candidates);
							printf("\n");
			
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
			
							printf("\nPropagation complete.\n\n");
						} else {
							/* If we didn't find enough matching nodes, this wasn't a valid tuple. */
							printf("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
							i + 1, j + 1);
						}

						/* Free the candidates list for the next iteration */
						free_list(candidates);
					} else {
						printf("\t - Cell [%d][%d] already propagated\n",
						i + 1, j + 1);
						free_list(candidates); /* Free candidates if we skip due to already propagated */
					}
				}
			}
		}
	}

	free(coord);
	printf("\n");

	return changed;
}

void propagate_row(struct node ***extended_grid, int n,
		   struct coordinates *coord, int n_coordinates, int value)
{
	int i, j;
	int row =
		coord[0].row; /* Propagating only the row, the value will be same for all coords */
	int skip;

	printf("\nPropagating value %d on row %d\n", value, row + 1);

	for (i = 0; i < n; ++i) {
		skip = 0; /* Flag to check if current cell [row][i] should be skipped */
		for (j = 0; j < n_coordinates; ++j) {
			/* Check if the current column index 'i' matches any of the coordinate columns */
			if (i == coord[j].column) {
				skip = 1; /* Mark this cell to be skipped */
				break; /* No need to check further coordinates for this cell */
			}
		}

		/* If the cell should not be skipped, proceed with deletion */
		if (!skip) {
			extended_grid[row][i] = delete_at_given_value(
				extended_grid[row][i], value);
		}
	}
}

void propagate_column(struct node ***extended_grid, int n,
	struct coordinates *coord, int n_coordinates, int value)
{
	int i, j;
	int column =
		coord[0].column; /* Propagating only the column, the value will be same for all coords */
	int skip;

	printf("\nPropagating value %d on column %d\n", value, column + 1);

	for (i = 0; i < n; ++i) {
		skip = 0; /* Flag to check if current cell [i][column] should be skipped */
		for (j = 0; j < n_coordinates; ++j) {
			/* Check if the current row index 'i' matches any of the coordinate rows */
			if (i == coord[j].row) {
				skip = 1; /* Mark this cell to be skipped */
				break; /* No need to check further coordinates for this cell */
			}
		}

		/* If the cell should not be skipped, proceed with deletion */
		if (!skip) {
			extended_grid[i][column] = delete_at_given_value(
				extended_grid[i][column], value);
		}
	}
}

void propagate_box(struct node ***extended_grid, int n,
	struct coordinates *coord, int n_coordinates, int value)
{
	int i, j;
	int k;
	int sqrt_n;
	int skip;

	int row_start, col_start;

	sqrt_n = (int)sqrt(n);
	row_start = (coord[0].row / sqrt_n) * sqrt_n;
        col_start = (coord[0].column / sqrt_n) * sqrt_n; 

	printf("\nPropagating value %d in its box\n", value);

	for (i = row_start; i < row_start + sqrt_n; ++i) {
		for (j = col_start; j < col_start + sqrt_n; ++j) {
			skip = 0;

			for (k = 0; k < n_coordinates; ++k) {
				if (i == coord[k].row && j == coord[k].column) {
					skip = 1;
					break;
				}
			}

			if (!skip) {
				extended_grid[i][j] = delete_at_given_value(extended_grid[i][j], value);
			}
		}
	}
}

int hidden_singles(struct node ***extended_grid, int n)
{
	int is_changed;
	int i, j; /* Loop variables */
	int flag;
	int value;

	is_changed = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			struct node *temp = extended_grid[i][j];

			/* If already a single value, go to next cell */
			if (temp->next == NULL) {
				printf("\tAt [%d][%d] already a single value: %d\n",
				       i + 1, j + 1, temp->data);
				continue;
			}

			/* Traverse every node of the list looking if it's a hidden single */
			flag = 0;
			while (temp != NULL) {
				value = temp->data;
				printf("\tAt [%d][%d] - checking value %d\n",
				       i + 1, j + 1, value);

				flag = check_hidden_single(extended_grid, n, i,
							   j, value);

				if (!flag) {
					printf("\t - Value %d is a hidden single\n",
					       value);
					break;
				} else {
					printf("\t - Copy found, value %d is not a hidden single, going to next node\n",
					       value);
				}

				temp = temp->next;
			}

			if (!flag) {
				struct node *temp2 = extended_grid[i][j];
				temp2->data = value;
				temp2 = delete_all_but_head(temp2);

				is_changed = 1;
			}

			printf("\n");
		}
	}

	return is_changed;
}

int check_hidden_single(struct node ***extended_grid, int n, int row, int col,
			int value)
{
	int i, j; /* Loop variables */
	int flag = 0; /* Flag to check if the value is present */
	int box_row, box_col; /* Row and column of the box */
	int sqrt_n; /* Square root of n */

	/* Check if the value is present in the same row */
	for (j = 0; j < n; j++) {
		if (j != col) {
			struct node *temp = extended_grid[row][j];
			while (temp != NULL) {
				if (temp->data == value) {
					flag = 1;
					return flag;
				}
				temp = temp->next;
			}
		}
	}

	/* Check if the value is present in the same column */
	if (!flag) {
		for (i = 0; i < n; i++) {
			if (i != row) {
				struct node *temp = extended_grid[i][col];
				while (temp != NULL) {
					if (temp->data == value) {
						flag = 1;
						return flag;
					}
					temp = temp->next;
				}
			}
		}
	}

	/* Check if the value is present in the same box */
	sqrt_n = (int)sqrt(n);
	box_row = row - (row % sqrt_n);
	box_col = col - (col % sqrt_n);

	for (i = box_row; i < box_row + sqrt_n; i++) {
		for (j = box_col; j < box_col + sqrt_n; j++) {
			struct node *temp = extended_grid[i][j];

			if (i != row && j != col) {
				while (temp != NULL) {
					if (temp->data == value) {
						flag = 1;
						return flag;
					}
					temp = temp->next;
				}
			}
		}
	}

	return flag;
}

void print_extended_grid(struct node ***extended_grid, int n)
{
	int i, j; /* Loop variables */

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			printf("At [%d][%d]: ", i + 1, j + 1);
			print_list(extended_grid[i][j]);
			printf("\n");
		}
		printf("\n");
	}
}

void free_extended_grid(struct node ***extended_grid, int n)
{
	int i, j; /* Loop variables */

	if (extended_grid != NULL) {
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++) {
				struct node *temp = extended_grid[i][j];
				free_list(temp);
			}

			free(extended_grid[i]);
		}

		free(extended_grid);
	}
}

void free_propagation_matrix(int ***propagation, int n)
{
	int i, j;
	int sqrt_n;

	sqrt_n = (int)sqrt(n);
	for (i = 0; i < sqrt_n; ++i) {
		for (j = 0; j < n; ++j) {
			free(propagation[i][j]);
		}
		free(propagation[i]);
	}
	free(propagation);
}
