#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/linked_list.h"
#include "../include/solver.h"
#include "../include/sudoku_utils.h"

int sudoku_solver(int **grid, int n)
{
	int i, j;		/* Loop variables */
	int is_changed;			/* Flag to check if any changes are made */
	struct node ***extended_grid;	/* Extended grid for constraint propagation */
	int counter;
	int numbers_left;
	int depth;
	int max_depth = 3; 

	int **already_propagated_single;
	int **already_propagated_pair;
	int **already_propagated_triple;
	int **selected_propagated;

	/* Initialize the already_propagated grid */
	initialize_propagation_grid(&already_propagated_single, n);
	initialize_propagation_grid(&already_propagated_pair, n);
	initialize_propagation_grid(&already_propagated_triple, n);

	/* Create an extended grid */
	extended_grid = extend_grid(grid, n);
	if (extended_grid == NULL) {
		fprintf(stderr, "Error: Unable to create extended grid\n");
		free_grid(grid, n);
		free(already_propagated_single);
		free(already_propagated_pair);
		free(already_propagated_triple);
		
		return -1;
	}

	/* Print the extended grid */
	printf("\nExtended grid:\n");
	print_extended_grid(extended_grid, n);

	/* Solve the Sudoku puzzle using constraint propagation */
	counter = 0;
	do {
		is_changed = 0;		/* Reset the flag for each iteration */

		for (depth = 1; depth <= max_depth; ++depth) {
			switch (depth)
			{
				case 1:
					selected_propagated = already_propagated_single;
					break;
				case 2:
					selected_propagated = already_propagated_pair;
					break;
				
				case 3:
					selected_propagated = already_propagated_triple;
					break;

				default:
					break;
			}

			/* Use the technique of simple elimination */
			printf("\nSimple elimination...\n");
			is_changed += simple_elimination(extended_grid, n, selected_propagated, depth);
		}
		
		/* Use technique of hidden singles */
		printf("\n\nHidden singles...\n");
		is_changed += hidden_singles(extended_grid, n);

		/* Use technique of naked pairs */
		printf("\n\nNaked pairs...\n");
		is_changed += naked_pair(extended_grid, n);

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
	printf("Progress: %2.1f%%\n", (double)((double)1 - (double)(numbers_left - n * n) / (double)((n * n * n) - (n * n))) * 100);
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
	free_grid(selected_propagated, n);

	return 0;
}

struct node ***extend_grid(int **grid, int n)
{
	struct node ***extended_grid; /* Extended grid */
	int i, j, k;		/* Loop variables */

	extended_grid = (struct node ***)malloc(n * sizeof(struct node **));
	if (extended_grid == NULL) {
		fprintf(stderr, "Error: Unable to allocate memory for extended grid\n");
		return NULL;
	}

	for (i = 0; i < n; i++) {
		extended_grid[i] = (struct node **)malloc(n * sizeof(struct node *));
		if (extended_grid[i] == NULL) {
			fprintf(stderr, "Error: Unable to allocate memory for extended grid row\n");
			for (j = 0; j < i; j++) {
				free(extended_grid[j]);
			}
			free(extended_grid);
			return NULL;
		}

		for (j = 0; j < n; j++) {
			extended_grid[i][j] = NULL;
			if (grid[i][j] != 0)
				extended_grid[i][j] = append(extended_grid[i][j], grid[i][j]);
			else
				for (k = 1; k <= n; k++)
					extended_grid[i][j] = append(extended_grid[i][j], k);

			
			if (extended_grid[i][j] == NULL) {
				fprintf(stderr, "Error: Unable to create or append node in extended grid\n");
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

void print_extended_grid(struct node ***extended_grid, int n)
{
	int i, j;		/* Loop variables */

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			printf("At [%d][%d]: ", i + 1, j + 1);
			print_list(extended_grid[i][j]);
			printf("\n");
		}
		printf("\n");
	}
}

void initialize_propagation_grid(int ***propagation, int n)
{
	int i, j;		/* Loop variable */

	*propagation = (int **)malloc(n * sizeof(int *));
	if (*propagation == NULL) {
		fprintf(stderr, "Error: Unable to allocate memory for propagation array\n");
		return;
	}

	for (i = 0; i < n; i++) {
		(*propagation)[i] = (int *)malloc(n * sizeof(int));
		if ((*propagation)[i] == NULL) {
			fprintf(stderr, "Error: Unable to allocate memory for propagation row\n");
			for (j = 0; j < i; j++) {
				free((*propagation)[j]);
			}
			free(*propagation);
			return;
		}

		for (j = 0; j < n; j++) {
			(*propagation)[i][j] = 0;
		}
	}


}

void propagate(struct node ***extended_grid, int n, struct coordinates *coord, int n_coord, int value)
{
	int i, j;		/* Loop variables */
	int r, c;		/* Row and column of the box */
	int sqrt_n;		/* Square root of n */

	/* Remove the value from the same row */
	for (i = 0; i < n; ++i) {
		int skip = 0; /* Flag to check if current row 'i' should be skipped */
		for (j = 0; j < n_coord; ++j) {
		    if (i == coord[j].row) {
			skip = 1; /* Mark this row to be skipped */
			break;
		    }
		}
		if (!skip) {
		    for (j = 0; j < n; ++j) {
			extended_grid[i][j] = delete_at_given_value(extended_grid[i][j], value);
		    }
		}
	    }

	/* Remove the value from the same column */
	for (j = 0; j < n; ++j) {
		int skip = 0; /* Flag to check if current column 'j' should be skipped */
		for (i = 0; i < n_coord; ++i) {
		    if (j == coord[i].column) {
			skip = 1; /* Mark this column to be skipped */
			break;
		    }
		}
		if (!skip) {
		    for (i = 0; i < n; ++i) {
			extended_grid[i][j] = delete_at_given_value(extended_grid[i][j], value);
		    }
		}
	    }

	/* Remove the value from the same box */
	sqrt_n = (int)sqrt(n);
	for (i = 0; i < n_coord; ++i) {
		int grid_row_start = (coord[i].row / sqrt_n) * sqrt_n;
		int grid_col_start = (coord[i].column / sqrt_n) * sqrt_n;
	
		for (r = grid_row_start; r < grid_row_start + sqrt_n; ++r) {
		    for (c = grid_col_start; c < grid_col_start + sqrt_n; ++c) {
			/* Skip the cells that are part of the naked tuple */
			int skip = 0;
			for (j = 0; j < n_coord; ++j) {
			    if (r == coord[j].row && c == coord[j].column) {
				skip = 1;
				break;
			    }
			}
			if (!skip) {
			    extended_grid[r][c] = delete_at_given_value(extended_grid[r][c], value);
			}
		    }
		}
	    }

}

int simple_elimination(struct node ***extended_grid, int n, int **already_propagated, int depth)
{
	int is_changed;
	int i, j, k;		/* Loop variables */
	int x;                  /* Loop variable to save the coordinates */
	int remaining_nodes;	/* Number of nodes left in the list */
	struct coordinates *coord;
	struct node *candidates = NULL; /* Initialize candidates to NULL */
	struct node *temp;
	struct node *temp2;
	
	printf("\nElimination at depth %d\n", depth);

	/* Set coordinates array to lenght depth */
	coord = (struct coordinates *)malloc(depth * sizeof(struct coordinates));
	if (coord == NULL) {
		printf("Memory allocation failed\n");
		return -1; /* Indicate error */
	}

	is_changed = 0;
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			remaining_nodes = depth;
			candidates = NULL;
			x = 0; /* Reset coordinate index for each potential starting node */

			temp = extended_grid[i][j];
			printf("\tAt [%d][%d]: ", i + 1, j + 1);
			print_list(temp);

			if (temp == NULL) {
				printf("\n\t - No values left in this cell\n");
				free_list(candidates); /* Free any potential remnants if loop continues */
				continue;
			}

			/* Check the right depth */
			if (size_list(temp) > depth) {
				printf("\t\t - More than %d values in this row\n", depth);
				free_list(candidates); /* Free any potential remnants if loop continues */
				continue;
			}

			/* If we are in this section it means we found something with a good depth */
			printf("\t\tRight number of values\n");

			if(!already_propagated[i][j]) {
				/* Append values in candidate list */
				temp2 = temp;
				do {
					candidates = append(candidates, temp2->data);
					if (candidates == NULL) {
						fprintf(stderr, "Failed to append node in elimination\n");
						free(coord);
						return -1; /* Indicate error */
					}
					temp2 = temp2->next;
				} while (temp2 != NULL);

				/* Save node coordinates */
				coord[x].row = i;
				coord[x].column = j;
				x++;

				/* Subtract from counter to signal the possible candidate */
				printf("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				printf("...%d\n", remaining_nodes);

				/* Search for other candidates if needed for the naked tuple */

				for (k = i + 1; k < n && remaining_nodes != 0; ++k) {
					temp2 = extended_grid[k][j];

					printf("\t\t\tChecking if %d matches %d...", k + 1, i + 1);
					if (check_same_list(candidates, temp2)) {
						printf("\t...they match\n");

						--remaining_nodes;

						/* Save node coordinates */
						coord[x].row = k;
						coord[x].column = j;
						x++;

						if (remaining_nodes == 0)
							break;
					} else {
						printf("\t...they DO NOT match\n");
					}
				}

				if (remaining_nodes == 0){
					printf("\nFound naked tuple of size %d at cell: ", depth);
					for (k = 0; k < depth; ++k) {
						printf("%d ", coord[k].row + 1);
						printf("%d ", coord[k].column + 1);
					}
					printf("\nValues to propagate: ");
					print_list(candidates);
					printf("\n");

					/* Propagate each value in the candidate list */
					temp2 = candidates;
					while (temp2 != NULL) {
						propagate(
							extended_grid, n, coord, depth, 
							temp2->data); /* Pass 'depth' as n_coordinates */
						temp2 = temp2->next;
					}

					/* Mark involved rows as propagated */
					for (k = 0; k < depth; ++k) {
						already_propagated[coord[k].row][coord[k].column] = 1;
					}
					is_changed = 1; /* Set the flag to indicate a change */

					printf("\nPropagation complete.\n\n");
				} else {
					printf("\t\t - No naked tuple found\n");
				}

				/* Free the candidates list for the next iteration */
				free_list(candidates); 
				candidates = NULL; /* Ensure it's NULL for the next iteration */
			} else {
				printf("\t\t - Already propagated\n");
				free_list(candidates); /* Free candidates if we skip due to already propagated */
				candidates = NULL;
			}

		} 
		
		printf("\n");
	}

	free(coord);
	return is_changed;
}

int hidden_singles(struct node ***extended_grid, int n)
{
	int is_changed;
	int i, j;		/* Loop variables */
	int flag;
	int value;

	is_changed = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			struct node *temp = extended_grid[i][j];

			/* If already a single value, go to next cell */
			if (temp->next == NULL) {
				printf("\tAt [%d][%d] already a single value: %d\n", i + 1, j + 1, temp->data);
				continue;
			}

			/* Traverse every node of the list looking if it's a hidden single */
			flag = 0;
			while (temp != NULL) {
				value = temp->data;
				printf("\tAt [%d][%d] - checking value %d\n", i + 1, j + 1, value);

				flag = check_hidden_single(extended_grid, n, i, j, value);
				
				if (!flag) {
					printf("\t - Value %d is a hidden single\n", value);
					break;
				} else {
					printf("\t - Copy found, value %d is not a hidden single, going to next node\n", value);
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

int check_hidden_single(struct node ***extended_grid, int n, int row, int col, int value)
{
	int i, j;		/* Loop variables */
	int flag = 0;		/* Flag to check if the value is present */
	int box_row, box_col;	/* Row and column of the box */
	int sqrt_n;		/* Square root of n */

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

int naked_pair(struct node ***extended_grid, int n)
{
	int is_changed, flag;
	int i, j;

	is_changed = 0;

	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			flag = 0;
			struct node *temp = extended_grid[i][j];

			if (temp->next != NULL && temp->next->next == NULL) {
				printf("Found a possible pair in [%d][%d]: ", i + 1, j + 1);
				print_list(temp);
				printf("\n");

				/* Check row and eventually propagate */
				flag = check_naked_pair(extended_grid, n, i, j);

				if (flag == 0) {
					printf("\t - No naked pair found\n\n");
				} else {
					printf("**************** Naked pair found ****************\n\n");
					is_changed = 1;
				}

				/* Check column and eventually propagate */

				/* Check box and eventually propagate */
			}
		}
	}

	return is_changed;
}

int check_naked_pair(struct node ***extended_grid, int n, int row, int col)
{
	int changed;
	int i, j, k, l;		/* Loop variables */
	struct node *original_pair = extended_grid[row][col];
	int sqrt_n;		/* Square root of n */
	int box_row, box_col;	/* Row and column of the box */
	
	changed = 0;

	/* Check row */
	printf("Checking row %d for naked pairs...\n", row + 1);
	for (j = col + 1; j < n; j++) {
		struct node *temp = extended_grid[row][j];
		if (temp->next != NULL && temp->next->next == NULL) {
			printf("\tFound another pair in [%d][%d]: ", row + 1, j + 1);
			print_list(temp);
			printf("\n");

			/* Check if same values */
			printf("\t - Checking if same values...\n");
			if (temp->data == original_pair->data && temp->next->data == original_pair->next->data) {
				printf("\t\t - Same values\n");
				changed = 1;

				/* Propagate */
				for (i = 0; i < n; i++) {
					if (i != col && i != j) {
						extended_grid[row][i] = delete_at_given_value(extended_grid[row][i], temp->data);
						extended_grid[row][i] = delete_at_given_value(extended_grid[row][i], temp->next->data);
					}
				}
			} else {
				printf("\t\t - Different values\n");
			}
		}
	}

	/* Check column */
	printf("Checking column %d for naked pairs...\n", col + 1);
	for (i = row + 1; i < n; i++) {
		struct node *temp = extended_grid[i][col];
		if (temp->next != NULL && temp->next->next == NULL) {
			printf("\tFound another pair in [%d][%d]: ", i + 1, col + 1);
			print_list(temp);
			printf("\n");

			/* Check if same values */
			printf("\t - Checking if same values...\n");
			if (temp->data == original_pair->data && temp->next->data == original_pair->next->data) {
				printf("\t\t - Same values\n");
				changed = 1;

				/* Propagate */
				for (j = 0; j < n; j++) {
					if (j != row && j != i) {
						extended_grid[j][col] = delete_at_given_value(extended_grid[j][col], temp->data);
						extended_grid[j][col] = delete_at_given_value(extended_grid[j][col], temp->next->data);
					}
				}
			} else {
				printf("\t\t - Different values\n");
			}
		}
	}

	/* Check box */
	sqrt_n = (int)sqrt(n);
	box_row = row - (row % sqrt_n);
	box_col = col - (col % sqrt_n);

	printf("Checking box from [%d][%d] to [%d][%d] for naked pairs...\n" \
		, box_row + 1, box_col + 1, box_row + sqrt_n, box_col + sqrt_n);
	for (i = box_row; i < box_row + sqrt_n; i++) {
		for (j = box_col; j < box_col + sqrt_n; j++) {
			if (i == row && j == col)
				continue;

			struct node *temp = extended_grid[i][j];

			if (temp->next != NULL && temp->next->next == NULL) {
				printf("\tFound another pair in [%d][%d]: ", i + 1, j + 1);
				print_list(temp);
				printf("\n");

				/* Check if same values */
				printf("\t - Checking if same values...\n");
				if (temp->data == original_pair->data && temp->next->data == original_pair->next->data) {
					printf("\t - Same values\n");

					changed = 1;

					/* Propagate */
					for (k = box_row; k < box_row + sqrt_n; k++) {
						for (l = box_col; l < box_col + sqrt_n; l++) {
							if (!((k == row && l == col) || (k == i && l == j))) {
								extended_grid[k][l] = delete_at_given_value(extended_grid[k][l], temp->data);
								extended_grid[k][l] = delete_at_given_value(extended_grid[k][l], temp->next->data);
							}
						}
					}
				} else {
					printf("\t - Different values\n");
				}
			}
		}
	}

	return changed;
}

void free_extended_grid(struct node ***extended_grid, int n)
{
	int i, j;		/* Loop variables */

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
