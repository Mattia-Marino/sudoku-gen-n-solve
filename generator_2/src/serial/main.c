/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/sudoku.h"
#include "../../include/solver.h"

int main(int argc, char **argv)
{
	int n;
	int num_puzzles;
	int count;
	int sqrt_n;
	int seed;
	struct Sudoku *sudoku;
	char filename[32];
	FILE *file;

	/* Check if we have the right number of arguments */
	if (argc != 3) {
		printf("Usage: %s <size> <number_of_puzzles>\n", argv[0]);
		return 1;
	}

	/* Parse the size from command line */
	n = atoi(argv[1]);
	num_puzzles = atoi(argv[2]);

	/* Check if n is a positive number */
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		return 1;
	}

	/* Calculate square root of n */
	sqrt_n = (int)sqrt(n);

	/* For Sudoku, n MUST be a perfect square (4, 9, 16, etc.) */
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		return 1;
	}

	/* Check if num_puzzles is in range */
	if (num_puzzles <= 0 || num_puzzles > 10000) {
		printf("Number of puzzles must be between 1 and 10000.\n");
		return 1;
	}

	/* Create output file */
	snprintf(filename, sizeof(filename), "output_%d_%d.txt", n, num_puzzles);
	file = fopen(filename, "w");

	if (file == NULL) {
		fprintf(stderr, "Error: Could not open %s for writing\n",
			filename);
		return 1;
	}


	for (count = 0; count < num_puzzles; ++count) {
		printf("Generating puzzle %d of %d...\n", count + 1, num_puzzles);

		/* Initialize the Sudoku grid */
		sudoku = initSudoku(n);

		/* Fill the first row with a random permutation */
		seed = time(NULL);
		insertFirstLine(sudoku, seed);

		/* Generate a complete Sudoku grid */
		SudokuSolver(sudoku);

		removeNumbers(sudoku);

		/* Save the Sudoku grid to a file */
		saveSudokuToFile(sudoku, file);

		/* If not the last puzzle, go to next row */
		if (count + 1 < num_puzzles)
			fprintf(file, "\n");

		/* Deallocate memory */
		destroySudoku(sudoku);
	}

	fclose(file);
	printf("\nSudoku puzzles saved to %s\n", filename);
	
	return 0;
}