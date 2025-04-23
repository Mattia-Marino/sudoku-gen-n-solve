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
	int **already_propagated;
	int numbers_left;

	/* Create an extended grid */
	extended_grid = extend_grid(grid, n);
	if (extended_grid == NULL) {
		fprintf(stderr, "Error: Unable to create extended grid\n");
		return -1;
	}

	/* Print the extended grid */
	printf("\nExtended grid:\n");
	print_extended_grid(extended_grid, n);

	/* Initialize the already_propagated array */
	already_propagated = (int **)malloc(n * sizeof(int *));
	if (already_propagated == NULL) {
		fprintf(stderr, "Error: Unable to allocate memory for already_propagated matrix\n");
		free(already_propagated);
		free_extended_grid(extended_grid, n);

		return -1;
	}

	for (i = 0; i < n; i++) {
		already_propagated[i] = (int *)malloc(n * sizeof(int));
		if (already_propagated[i] == NULL) {
			fprintf(stderr, "Error: Unable to allocate memory for already_propagated row\n");
			for (j = 0; j < i; j++) {
				free(already_propagated[j]);
			}
			free(already_propagated);
			free_extended_grid(extended_grid, n);

			return -1;
		}

		for (j = 0; j < n; j++) {
			already_propagated[i][j] = 0;
		}
	}

	/* Print the already_propagated matrix */
	printf("\nAlready propagated matrix:\n");
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			printf("%d ", already_propagated[i][j]);
		}
		printf("\n");
	}

	/* Solve the Sudoku puzzle using constraint propagation */
	counter = 0;
	do {
		is_changed = 0;		/* Reset the flag for each iteration */

		/* Use the technique of simple elimination */
		printf("\nSimple elimination...\n");
		is_changed += simple_elimination(extended_grid, n, already_propagated);
		
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

void propagate(struct node ***extended_grid, int n, int row, int col, int value)
{
	int i, j;		/* Loop variables */
	int sqrt_n;		/* Square root of n */
	int box_row, box_col;	/* Row and column of the box */

	/* Remove the value from the same row */
	printf("\nPropagating value %d from [%d][%d] on row %d\n", value, row + 1, col + 1, row + 1);
	for (j = 0; j < n; j++) {
		if (j != col) {
			extended_grid[row][j] = delete_at_given_value(extended_grid[row][j], value);
		}
	}

	/* Remove the value from the same column */
	printf("Propagating value %d from [%d][%d] on column %d\n", value, row + 1, col + 1, col + 1);
	for (i = 0; i < n; i++) {
		if (i != row) {
			extended_grid[i][col] = delete_at_given_value(extended_grid[i][col], value);
		}
	}

	/* Remove the value from the same box */
	sqrt_n = (int)sqrt(n);
	box_row = row - (row % sqrt_n);
	box_col = col - (col % sqrt_n);

	printf("Propagating value %d from [%d][%d] on box from [%d][%d] to [%d][%d]\n" \
		, value, row + 1, col + 1, box_row + 1, box_col + 1, box_row + sqrt_n, box_col + sqrt_n);
	for (i = box_row; i < box_row + sqrt_n; i++) {
		for (j = box_col; j < box_col + sqrt_n; j++) {
			if (i != row && j != col) {
				extended_grid[i][j] = delete_at_given_value(extended_grid[i][j], value);
			}
		}
	}

}

int simple_elimination(struct node ***extended_grid, int n, int **already_propagated)
{
	int is_changed;
	int i, j;		/* Loop variables */
	int value;		/* Value to be propagated */

	is_changed = 0;
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			struct node *temp = extended_grid[i][j];
			printf("\tAt [%d][%d]: ", i + 1, j + 1);
			print_list(temp);

			if (temp == NULL) {
				printf("\n\t - No values left in this cell\n");
				continue;
			}

			if (is_last_node(temp)) {
				printf("\n\t - Only one value left in this cell: %d", temp->data);
			} else {
				printf("\n\t - More than one value left in this cell");
			}

			/* If the cell has only one value, remove it from the row and column */
			if (is_last_node(temp) && !already_propagated[i][j]) {
				value = temp->data;
				
				/* Propagate */
				propagate(extended_grid, n, i, j, value);

				already_propagated[i][j] = 1;
				is_changed = 1;
			} else {
				printf("\n\t - Nothing to propagate here\n");
			}
		}
		printf("\n");
	}

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
