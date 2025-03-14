// SPDX-License-Identifier: GPL-3.0

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

// Function declarations
bool isSafe(int **grid, int row, int col, int num, int n, int subrow,
	    int subcol);
bool solveSudoku(int **grid, int n, int subrow, int subcol);
void printGrid(int **grid, int n);
int **createGrid(int n);
void freeGrid(int **grid, int n);

int main(int argc, char *argv[])
{
	// Check if we have the right number of arguments
	if (argc != 2) {
		printf("Usage: %s <size>\n", argv[0]);
		return 1;
	}

	// Parse the size from command line
	int n = atoi(argv[1]);

	// Check if n is a positive number
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		return 1;
	}

	// Calculate square root of n
	int sqrt_n = (int)sqrt(n);

	// For Sudoku, n MUST be a perfect square (4, 9, 16, etc.)
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		return 1;
	}

	// Use square root as dimensions for sub-grids
	int subrow = sqrt_n;
	int subcol = sqrt_n;

	// Seed the random number generator
	srand(time(NULL));

	// Create the grid
	int **grid = createGrid(n);

	// Start timing the computation
	clock_t start_time = clock();

	// Generate a solved Sudoku puzzle
	bool success = solveSudoku(grid, n, subrow, subcol);

	// End timing
	clock_t end_time = clock();
	double computation_time =
		(double)(end_time - start_time) / CLOCKS_PER_SEC;

	if (success) {
		printf("Generated Sudoku grid of size %dx%d:\n", n, n);
		printGrid(grid, n);
		printf("\nComputation completed in %.6f seconds.\n",
		       computation_time);
	} else {
		printf("Failed to generate a valid Sudoku grid.\n");
		printf("Time spent attempting: %.6f seconds.\n",
		       computation_time);
	}

	// Free allocated memory
	freeGrid(grid, n);

	return 0;
}

// Create an n x n grid initialized to zeros
int **createGrid(int n)
{
	int **grid = (int **)malloc(n * sizeof(int *));
	for (int i = 0; i < n; i++) {
		grid[i] = (int *)malloc(n * sizeof(int));
		for (int j = 0; j < n; j++) {
			grid[i][j] = 0;
		}
	}
	return grid;
}

// Free the allocated memory for the grid
void freeGrid(int **grid, int n)
{
	for (int i = 0; i < n; i++) {
		free(grid[i]);
	}
	free(grid);
}

// Print the Sudoku grid
void printGrid(int **grid, int n)
{
	int sqrt_n = (int)sqrt(n);

	for (int i = 0; i < n; i++) {
		// Print horizontal line
		if (i % sqrt_n == 0 && i != 0) {
			for (int j = 0; j < n; j++) {
				printf("---");
				if ((j + 1) % sqrt_n == 0 && (j + 1) < n)
					printf("+");
			}
			printf("\n");
		}

		for (int j = 0; j < n; j++) {
			// Print vertical line
			if (j % sqrt_n == 0 && j != 0)
				printf("|");

			// Print the cell content or a dot for empty cells
			if (grid[i][j] == 0) {
				printf(" . ");
			} else {
				printf("%2d ", grid[i][j]);
			}
		}
		printf("\n");
	}
}

// Check if it's safe to place a number at a specific position
bool isSafe(int **grid, int row, int col, int num, int n, int subrow,
	    int subcol)
{
	// Check if 'num' is not already placed in current row
	for (int x = 0; x < n; x++) {
		if (grid[row][x] == num) {
			return false;
		}
	}

	// Check if 'num' is not already placed in current column
	for (int x = 0; x < n; x++) {
		if (grid[x][col] == num) {
			return false;
		}
	}

	// Check if 'num' is not already placed in current sub-grid
	int startRow = row - row % subrow;
	int startCol = col - col % subcol;

	for (int i = 0; i < subrow; i++) {
		for (int j = 0; j < subcol; j++) {
			if (grid[i + startRow][j + startCol] == num) {
				return false;
			}
		}
	}

	return true;
}

// Solve the Sudoku grid using backtracking
bool solveSudoku(int **grid, int n, int subrow, int subcol)
{
	int row = 0, col = 0;
	bool isEmpty = false;

	// Find an empty position
	for (row = 0; row < n; row++) {
		for (col = 0; col < n; col++) {
			if (grid[row][col] == 0) {
				isEmpty = true;
				break;
			}
		}
		if (isEmpty) {
			break;
		}
	}

	// If no empty position is found, we've solved the puzzle
	if (!isEmpty) {
		return true;
	}

	// Create an array with numbers 1 to n and shuffle it for random generation
	int *numbers = (int *)malloc(n * sizeof(int));
	for (int i = 0; i < n; i++) {
		numbers[i] = i + 1;
	}

	// Simple Fisher-Yates shuffle
	for (int i = n - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		int temp = numbers[i];
		numbers[i] = numbers[j];
		numbers[j] = temp;
	}

	// Try different numbers at the current empty position
	for (int num_idx = 0; num_idx < n; num_idx++) {
		int num = numbers[num_idx];

		// Check if 'num' can be placed at (row, col)
		if (isSafe(grid, row, col, num, n, subrow, subcol)) {
			// Place 'num' at (row, col)
			grid[row][col] = num;

			// Recur to fill the rest of the grid
			if (solveSudoku(grid, n, subrow, subcol)) {
				free(numbers);
				return true;
			}

			// If placing 'num' doesn't lead to a solution, reset and try another number
			grid[row][col] = 0;
		}
	}

	free(numbers);
	// If no number can be placed, backtrack
	return false;
}