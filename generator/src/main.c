// SPDX-License-Identifier: GPL-3.0

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/sudoku.h"
#include "../include/solver.h"

int main(int argc, char **argv)
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

	// Initialize the Sudoku grid
	printf("Initializing Sudoku grid of size %dx%d...\n", n, n);
	struct Sudoku *sudoku = initSudoku(n);

	// Display the empty Sudoku grid
	printf("\nEmpty Sudoku grid:\n");
	displaySudoku(sudoku);
	printf("\n\n\n");

	// Start timing the computation
	clock_t start_time = clock();

	// Fill the first row with a random permutation
	insertFirstLine(sudoku);

	// Generate a complete Sudoku grid
	printf("Generating a complete Sudoku grid...\n\n");
	SudokuSolver(sudoku);
	printf("The proposed grid:\n");
	displaySudoku(sudoku);

	clock_t mid_time = clock();
	double solving_time =
		(double)(mid_time - start_time) / CLOCKS_PER_SEC;
	printf("\nComplete board generated in %.6f seconds.\n", solving_time);

	printf("\n\n\n");
	printf("Now generating a playable board from the given solution...\n\n");
	removeNumbers(sudoku);
	printf("The playable grid:\n");
	displaySudoku(sudoku);

	// End timing
	clock_t end_time = clock();

	double generating_time =
		(double)(end_time - mid_time) / CLOCKS_PER_SEC;
	printf("\nGenerating playable board completed in %.6f seconds.\n", generating_time);

	double computation_time =
		(double)(end_time - start_time) / CLOCKS_PER_SEC;

	printf("\nTotal computation completed in %.6f seconds.\n", computation_time);

	// Save the Sudoku grid to a file
	saveSudokuToFile(sudoku);

	// Deallocate memory
	destroySudoku(sudoku);
	return 0;
}
