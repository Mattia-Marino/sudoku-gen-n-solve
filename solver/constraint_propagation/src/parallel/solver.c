#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

	#include "../../include/debug.h"
	#include "../../include/linked_list.h"
	#include "../../include/solver.h"
	#include "../../include/solver_parallel.h"
	#include "../../include/sudoku_utils.h"

int parallel_sudoku_solver(int **grid, int n, int rank, int size)
{
	int i, j; /* Loop variables */
	int is_changed; /* Flag to check if any changes are made */
	int depth;
	int max_depth;
	int numbers_left;
	struct node ***extended_grid; /* Extended grid for constraint propagation */

	int ***already_propagated_rows;
	int ***already_propagated_columns;
	int ***already_propagated_boxes;
	int **selected_propagated;

	int sqrt_n;
	int starting_row, ending_row;

	sqrt_n = (int)sqrt(n);
	starting_row = (rank % sqrt_n) * sqrt_n;
	ending_row = starting_row + sqrt_n;

	DPRINTF("I am process %d. Starting row: %d. Ending row: %d\n\n", rank, starting_row, ending_row - 1);

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
	extended_grid = extend_grid(grid, n);
	if (extended_grid == NULL) {
		fprintf(stderr, "Error: Unable to create extended grid\n");
		return -1;
	}

	/* Print the extended grid */
	DPRINTF("\nExtended grid:\n");
	DPRINT_EXTENDED_GRID(extended_grid, n);	

	/* Solve the Sudoku puzzle using constraint propagation */
	do {
		is_changed = 0; /* Reset the flag for each iteration */

		/* Use the technique of naked candidates */
		for (depth = 1; depth <= max_depth; ++depth) {
			selected_propagated = already_propagated_rows[depth - 1];
			is_changed += parallel_naked_candidates_rows(extended_grid,
				n, selected_propagated, depth,
				starting_row, ending_row);
			
			DPRINTF("\n\nPropagation at depth (row): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");

			/*
			selected_propagated = already_propagated_columns[depth - 1];
			is_changed += naked_candidates_columns(extended_grid,
				n, selected_propagated, depth);
		
			DPRINTF("\n\nPropagation at depth (col): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");
			
			selected_propagated = already_propagated_boxes[depth - 1];
			is_changed += naked_candidates_boxes(extended_grid,
				n, selected_propagated, depth);
			
			DPRINTF("\n\nPropagation at depth (box): %d\n", depth);
			DPRINT_EXTENDED_GRID(extended_grid, n);
			DPRINTF("\n\n\n");
			*/
		}

		/* Use technique of hidden singles */
		/*DPRINTF("\n\nHidden singles...\n");
		is_changed += hidden_singles(extended_grid, n);*/

		/* Print the updated extended grid */
		/*DPRINTF("\nUpdated extended grid:\n");
		DPRINT_EXTENDED_GRID(extended_grid, n);
		DPRINTF("\n\n\n");*/

		MPI_Barrier(MPI_COMM_WORLD);
		communication(extended_grid, n, rank, size);
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
	DPRINTF("Numbers left in the extended grid: %d\n", numbers_left);
	DPRINTF("Progress: %2.1f%%\n",
	       (double)((double)1 - (double)(numbers_left - n * n) /
					    (double)((n * n * n) - (n * n))) *
		       100);

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

void communication(struct node ***extended_grid, int n, int rank, int size)
{
	int i, j;

	/* Variables for MPI processing of EACH list in 'row' */
	int local_list_size;
	int *local_list_data;
	struct node *current_node_in_original_list; /* Iterator for the original list row[list_idx] */
	int *all_sizes;
	int *displacements;
	int total_flat_size;
	int *all_lists_flat_data;
	struct node *new_filtered_list_head; /* Head of the new filtered list for row[list_idx] */
	int data_to_check;
	int present_in_all_others; /* Using int for boolean: 1 for true, 0 for false */
	int other_rank_idx;
	int found_in_this_other_list; /* Using int for boolean */
	int other_list_start_idx;
	int other_list_current_size;
	int k; /* Loop counter for iterating through other lists' data */


	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			/* Reset variables for the current list row[list_idx] */
			local_list_data = NULL;
			all_sizes = NULL;
			displacements = NULL;
			all_lists_flat_data = NULL;
			new_filtered_list_head = NULL;
			total_flat_size = 0;
			current_node_in_original_list = extended_grid[i][j];

			/* 1. Serialize local list (row[list_idx]) into an array */
			local_list_size = size_list(current_node_in_original_list);
			if (local_list_size > 0) {
				local_list_data =
					(int *)malloc(local_list_size * sizeof(int));
				if (local_list_data == NULL) {
					fprintf(stderr,
						"Process %d: Failed to allocate memory for local_list_data (list [%d][%d])\n",
						rank, i, j);
					MPI_Abort(MPI_COMM_WORLD, 1);
				}
				/* Populate local_list_data from current_node_in_original_list (which is row[list_idx]) */
				struct node *temp_iter = current_node_in_original_list;
				for (k = 0; k < local_list_size; k++) {
					local_list_data[k] = temp_iter->data;
					temp_iter = temp_iter->next;
				}
			}

			/* 2. Gather all list sizes (for current row[list_idx] across processes) */
			all_sizes = (int *)malloc(size * sizeof(int));
			if (all_sizes == NULL) {
				fprintf(stderr,
					"Process %d: Failed to allocate memory for all_sizes (list [%d][%d])\n",
					rank, i, j);
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
			MPI_Allgather(&local_list_size, 1, MPI_INT, all_sizes, 1,
				MPI_INT, MPI_COMM_WORLD);

			/* 3. Prepare for Allgatherv: calculate displacements and total size */
			displacements = (int *)malloc(size * sizeof(int));
			if (displacements == NULL) {
				fprintf(stderr,
					"Process %d: Failed to allocate memory for displacements (list [%d][%d])\n",
					rank, i, j);
				MPI_Abort(MPI_COMM_WORLD, 1);
			}

			total_flat_size = 0;
			for (k = 0; k < size; k++) {
				total_flat_size += all_sizes[k];
			}
			if (size > 0) {
				displacements[0] = 0;
				for (k = 1; k < size; k++) {
					displacements[k] =
						displacements[k - 1] + all_sizes[k - 1];
				}
			}

			if (total_flat_size > 0) {
				all_lists_flat_data =
					(int *)malloc(total_flat_size * sizeof(int));
				if (all_lists_flat_data == NULL) {
					fprintf(stderr,
						"Process %d: Failed to allocate memory for all_lists_flat_data (list [%d][%d])\n",
						rank, i, j);
					MPI_Abort(MPI_COMM_WORLD, 1);
				}
			}

			/* 4. Gather all list data (for current row[list_idx] across processes) */
			if (total_flat_size > 0) {
				MPI_Allgatherv(local_list_data ? local_list_data :
								MPI_BOTTOM,
					local_list_size, MPI_INT,
					all_lists_flat_data, all_sizes,
					displacements, MPI_INT, MPI_COMM_WORLD);
			}

			/* 5. Identify elements to keep and build the new list (for current row[list_idx]) */
			/* current_node_in_original_list is already pointing to row[list_idx] */
			while (current_node_in_original_list != NULL) {
				data_to_check = current_node_in_original_list->data;
				present_in_all_others = 1; /* Assume true */

				if (size > 1) {
					for (other_rank_idx = 0; other_rank_idx < size;
					other_rank_idx++) {
						if (other_rank_idx == rank) {
							continue;
						}

						found_in_this_other_list = 0;
						other_list_current_size =
							all_sizes[other_rank_idx];

						if (other_list_current_size > 0 &&
						all_lists_flat_data != NULL) {
							other_list_start_idx =
								displacements
									[other_rank_idx];
							for (k = 0;
							k <
							other_list_current_size;
							k++) {
								if (all_lists_flat_data
									[other_list_start_idx +
									k] ==
								data_to_check) {
									found_in_this_other_list =
										1;
									break;
								}
							}
						} /* If other_list_current_size is 0, found_in_this_other_list remains 0 */

						if (!found_in_this_other_list) {
							present_in_all_others = 0;
							break;
						}
					}
				} /* If size == 1, present_in_all_others remains 1, all elements kept */

				if (present_in_all_others) {
					new_filtered_list_head = append(
						new_filtered_list_head, data_to_check);
				}
				current_node_in_original_list =
					current_node_in_original_list->next;
			}

			/* 6. Replace old list (row[list_idx]) with the new list */
			free_list(extended_grid[i][j]);
			extended_grid[i][j] = new_filtered_list_head;

			/* 7. Free allocated MPI-related memory for this iteration */
			if (local_list_data != NULL) {
				free(local_list_data);
			}
			free(all_sizes); /* all_sizes is always allocated if size > 0 */
			free(displacements); /* displacements is always allocated if size > 0 */
			if (all_lists_flat_data != NULL) {
				free(all_lists_flat_data);
			}
		}
	}
}
