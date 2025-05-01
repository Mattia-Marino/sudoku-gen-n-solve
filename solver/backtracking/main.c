/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int read_size_from_file(const char *filename);
int **create_grid(int n);
void free_grid(int **grid, int n);
int read_grid_from_file(int **grid, const char *filename);
void display_sudoku(int **grid, int n);
int is_safe(int **grid, int row, int col, int num, int n, int subrow,
	int subcol);
int solve_sudoku(int **grid, int n, int subrow, int subcol);

int main(int argc, char **argv)
{
	char *filename;
	int i, j; /* Loop variables */
	int n;
	int subrow, subcol; /* Sub-grid dimensions */
	int **grid;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Check if the correct number of arguments is passed */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[1];

	/* Read the size of the Sudoku grid from the file */
	n = read_size_from_file(filename);
	if (n == 0) {
		fprintf(stderr, "Error: Invalid file format\n");
		return 1;
	}

	/* Allocate memory for the Sudoku grid */
	grid = create_grid(n);
	if (grid == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory for grid\n");
		return 1;
	}

	/* Read the Sudoku grid from the file */
	if (read_grid_from_file(grid, filename) != 0) {
		fprintf(stderr, "Error: Failed to read grid from file\n");
		for (i = 0; i < n; i++) {
			free(grid[i]);
		}
		free(grid);
		return 1;
	}

	/* Display the given Sudoku grid */
	printf("\nGiven Sudoku grid:\n");
	display_sudoku(grid, n);
	printf("\n\n\n");

	/* Start timing the computation */
	start_time = clock();

	/* Generate a complete Sudoku grid */
	subrow = (int)sqrt(n);
	subcol = (int)sqrt(n);

	printf("Solving the sudoku...\n\n");
	solve_sudoku(grid, n, subrow, subcol);
	printf("The proposed grid:\n");
	display_sudoku(grid, n);

	/* End timing */
	end_time = clock();
	computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
	printf("\nTotal computation completed in %.6f seconds.\n",
	       computation_time);

	/* Free allocated memory */
	free_grid(grid, n);

	return 0;
}

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

	/* Create grid with the appropriate size */
	/*
	grid = create_grid(n);

	if (grid == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory for grid\n");
		fclose(file);
		return -1;
	}
		*/

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

/* Check if it's safe to place a number at a specific position */
int is_safe(int **grid, int row, int col, int num, int n, int subrow,
	    int subcol)
{
	int x;
	int i, j;
	int startRow;
	int startCol;

	/* Check if 'num' is not already placed in current row */
	for (x = 0; x < n; x++) {
		if (grid[row][x] == num) {
			return 0;
		}
	}

	/* Check if 'num' is not already placed in current column */
	for (x = 0; x < n; x++) {
		if (grid[x][col] == num) {
			return 0;
		}
	}

	/* Check if 'num' is not already placed in current sub-grid */
	startRow = row - row % subrow;
	startCol = col - col % subcol;

	for (i = 0; i < subrow; i++) {
		for (j = 0; j < subcol; j++) {
			if (grid[i + startRow][j + startCol] == num) {
				return 0;
			}
		}
	}

	return 1;
}

/* Solve the Sudoku grid using backtracking */
int solve_sudoku(int **grid, int n, int subrow, int subcol)
{
	int row = 0, col = 0;
	int isEmpty = 0;
	int *numbers;
	int i, j, temp, num_idx, num;

	/* Find an empty position */
	for (row = 0; row < n; row++) {
		for (col = 0; col < n; col++) {
			if (grid[row][col] == 0) {
				isEmpty = 1;
				break;
			}
		}
		if (isEmpty) {
			break;
		}
	}

	/* If no empty position is found, we've solved the puzzle */
	if (!isEmpty) {
		return 1;
	}

	/* Create an array with numbers 1 to n and shuffle it for random generation */
	numbers = (int *)malloc(n * sizeof(int));
	for (i = 0; i < n; i++) {
		numbers[i] = i + 1;
	}

	/* Simple Fisher-Yates shuffle */
	for (i = n - 1; i > 0; i--) {
		j = rand() % (i + 1);
		temp = numbers[i];
		numbers[i] = numbers[j];
		numbers[j] = temp;
	}

	/* Try different numbers at the current empty position */
	for (num_idx = 0; num_idx < n; num_idx++) {
		num = numbers[num_idx];

		/* Check if 'num' can be placed at (row, col) */
		if (is_safe(grid, row, col, num, n, subrow, subcol)) {
			/* Place 'num' at (row, col) */
			grid[row][col] = num;

			/* Recur to fill the rest of the grid */
			if (solve_sudoku(grid, n, subrow, subcol)) {
				free(numbers);
				return 1;
			}

			/* If placing 'num' doesn't lead to a solution, reset and try another number */
			grid[row][col] = 0;
		}
	}

	free(numbers);
	/* If no number can be placed, backtrack */
	return 0;
}