/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../include/sudoku.h"
#include "../include/solver.h"

int main(int argc, char **argv)
{
	char *filename;
	int n;
	struct Sudoku *sudoku;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Check if we have the right number of arguments */
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[1];

	n = readSizeFromFile(filename);
	if (n == 0) {
		printf("Error: Invalid file format\n");
		return 1;
	}
	sudoku = initSudoku(n);

	/* Read sudoku grid from file */
	readGridFromFile(sudoku, filename);

	/* Display the given Sudoku grid */
	printf("\nGiven Sudoku grid:\n");
	displaySudoku(sudoku);
	printf("\n\n\n");

	/* Start timing the computation */
	start_time = clock();

	/* Generate a complete Sudoku grid */
	printf("Solving the sudoku...\n\n");
	SudokuSolver(sudoku);
	printf("The proposed grid:\n");
	displaySudoku(sudoku);

	/* End timing */
	end_time = clock();

	computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	printf("\nTotal computation completed in %.6f seconds.\n",
	       computation_time);

	/* Deallocate memory */
	destroySudoku(sudoku);
	return 0;
}
