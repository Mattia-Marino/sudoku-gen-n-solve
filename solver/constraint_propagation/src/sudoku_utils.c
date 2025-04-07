#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "../include/sudoku_utils.h"

int read_size_from_file(const char *filename)
{
	FILE *file; /* File pointer */
	int n; /* Size of the Sudoku grid */
	int sqrt_n; /* Square root of n */

	/* Open the file for reading */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return 0;
	}

	/* Read grid size from first line */
	if (fscanf(file, "%d", &n) != 1) {
		fprintf(stderr, "Error: Invalid file format\n");
		fclose(file);
		return 0;
	}

	/* Check if n is a positive number */
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		fclose(file);
		return 0;
	}

	/* Calculate square root of n */
	sqrt_n = (int)sqrt(n);

	/* For Sudoku, n MUST be a perfect square (4, 9, 16, etc.) */
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		fclose(file);
		return 0;
	}

	fclose(file);
	return n;
}

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

int read_grid_from_file(int **grid, const char *filename)
{
	FILE *file;
	int n;
	int sqrt_n;
	int i, j;

	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return -1;
	}

	/* Read grid size from first line */
	if (fscanf(file, "%d", &n) != 1) {
		fprintf(stderr, "Error: Invalid file format\n");
		fclose(file);
		return -1;
	}

	/* Check if n is a positive number */
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		fclose(file);
		return -1;
	}

	/* Calculate square root of n */
	sqrt_n = (int)sqrt(n);

	/* For Sudoku, n MUST be a perfect square (4, 9, 16, etc.) */
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		fclose(file);
		return -1;
	}

	/* Read values and fill the grid */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (fscanf(file, "%d", &grid[i][j]) != 1) {
				fprintf(stderr,
					"Error: Invalid grid data at position [%d][%d]\n",
					i, j);
				free_grid(grid, n);
				fclose(file);
				return -1;
			}
		}
	}

	/* Close the file */
	fclose(file);
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