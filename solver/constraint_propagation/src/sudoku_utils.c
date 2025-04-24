#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../include/sudoku_utils.h"

int **create_grid(int n)
{
	int i, j; /* Loop variables */
	int **grid;

	grid = (int **)malloc(n * sizeof(int *));
	for (i = 0; i < n; i++) {
		grid[i] = (int *)malloc(n * sizeof(int));
		for (j = 0; j < n; j++) {
			grid[i][j] = 0;
		}
	}
	return grid;
}

void free_grid(int **grid, int n)
{
	int i; /* Loop variable */

	for (i = 0; i < n; i++) {
		free(grid[i]);
	}
	free(grid);
}

int read_grid_from_file(int **grid, FILE *file, int n)
{
	int i, j;

	/* Read values and fill the grid */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (fscanf(file, "%d", &grid[i][j]) != 1) {
				if (feof(file))
					return 1;

				fprintf(stderr,
					"Error: Invalid grid data at position [%d][%d]\n",
					i, j);
				return -1;
			}
		}
	}

	return 0;
}

void display_sudoku(int **grid, int n)
{
	int i, j; /* Loop variables */
	int sqrt_n;

	sqrt_n = (int)sqrt(n);
	for (i = 0; i < n; i++) {
		/* Print horizontal line */
		if (i % sqrt_n == 0 && i != 0) {
			for (j = 0; j < n; j++) {
				printf("---");
				if ((j + 1) % sqrt_n == 0 && (j + 1) < n)
					printf("+");
			}
			printf("\n");
		}

		for (j = 0; j < n; j++) {
			/* Print vertical line */
			if (j % sqrt_n == 0 && j != 0)
				printf("|");

			/* Print the cell content or a dot for empty cells */
			if (grid[i][j] == 0) {
				printf(" . ");
			} else {
				printf("%2d ", grid[i][j]);
			}
		}
		printf("\n");
	}
}