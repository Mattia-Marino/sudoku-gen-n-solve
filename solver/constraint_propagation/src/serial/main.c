/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/solver.h"
#include "../../include/sudoku_utils.h"

int main(int argc, char **argv)
{
	char *filename;
	int i;		/* Loop variables */
	int n;
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

	printf("Solving the sudoku...\n\n");
	sudoku_solver(grid, n);
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
