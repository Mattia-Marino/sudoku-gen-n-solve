/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/debug.h"
#include "../../include/solver.h"
#include "../../include/sudoku_utils.h"

int main(int argc, char **argv)
{
	char *filename;
	int n;
	int sqrt_n;
	int read_status;
	int tot_solved;
	int **grid;
	FILE *file;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	int rank, size;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Check if the correct number of arguments is passed by all processes */
	if (argc != 3) {
		if (rank == 0) {
			fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);
		}
		MPI_Finalize();
		return 1;
	}

	/* Read the size of the file from command line */
	n = atoi(argv[1]);

	/* Checks on puzzle size */
	if (n < 1) {
		fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
		return 1;
	}

	sqrt_n = (int)sqrt(n);
	if (sqrt_n * sqrt_n != n) {
		fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[2];

	/* Open the file */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return 1;
	}

	/* Start timing the computation */
	start_time = clock();

	tot_solved = 0;
	while (1) {
		/* Allocate memory for the Sudoku grid */
		grid = create_grid(n);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			fclose(file);
			return 1;
		}

		/* Read the Sudoku grid from the file */
		read_status = read_grid_from_file(grid, file, n);

		if (read_status != 0) {
			/* If read failed because we reached EOF */
			if (feof(file)) {
				DPRINTF("Reached EOF\n");
				break;
			} else {
				fprintf(stderr, "Error: Failed to read grid from file\n");
				free_grid(grid, n);
				fclose(file);
				return 1;
			}
		}

		/* Display the given Sudoku grid */
		DPRINTF("\nGiven Sudoku grid:\n");
		DPRINT_SUDOKU(grid, n);
		DPRINTF("\n\n\n");

		/* Solve the sudoku */
		DPRINTF("Solving the sudoku...\n\n");
		sudoku_solver(grid, n);
		DPRINTF("The proposed grid:\n");
		DPRINT_SUDOKU(grid, n);

		DPRINTF("\n\n--------------------\n\n");

		if (check_solved(grid, n))
			++tot_solved;

		/* Free the grid */
		free_grid(grid, n);
	}

	/* End timing */
	end_time = clock();
	computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
	printf("\nTotal computation completed in %.6f seconds.\n",
	       computation_time);

	printf("Sudokus completely solved: %d\n\n", tot_solved);

	/* Free allocated resources */
	fclose(file);
	
	MPI_Finalize();
	return 0;
}
