/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/debug.h"
#include "../../include/solver_parallel.h"
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
		if (rank == 0)
			fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);

		MPI_Finalize();
		return 1;
	}

	/* Read the size of the file from command line */
	n = atoi(argv[1]);

	/* Checks on puzzle size */
	if (n < 1) {
		if (rank == 0)
			fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
		
		MPI_Finalize();
		return 1;
	}

	sqrt_n = (int)sqrt(n);
	if (sqrt_n * sqrt_n != n) {
		if (rank == 0)
			fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
		
		MPI_Finalize();
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[2];

	/* Open the file */
	file = fopen(filename, "r");
	if (file == NULL) {
		if (rank == 0)
			fprintf(stderr, "Error: Unable to open file %s\n", filename);

		MPI_Finalize();
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
				DPRINTF("Process %d - Reached EOF\n", rank);
				break;
			} else {
				fprintf(stderr, "Process %d - Error: Failed to read grid from file\n", rank);
	
				free_grid(grid, n);
				fclose(file);
				
				MPI_Finalize();
				return 1;
			}
		}

		/* Rank 0 - Display the given Sudoku grid */
		if (rank == 0) {
			DPRINTF("\nGiven Sudoku grid:\n");
			DPRINT_SUDOKU(grid, n);
			DPRINTF("\n\n\n");
		}

		/* Solve the sudoku */
		DPRINTF("Process %d - Solving the sudoku...\n\n", rank);
		parallel_sudoku_solver(grid, n, rank, size);

		/* TODO: delete in prod to save time */
		fflush(stdout);
		MPI_Barrier(MPI_COMM_WORLD);

		if (rank == 0) {
			DPRINTF("The proposed grid:\n");
			DPRINT_SUDOKU(grid, n);

			DPRINTF("\n\n--------------------\n\n");
		}

		/* TODO: delete in prod to save time */
		fflush(stdout);
		MPI_Barrier(MPI_COMM_WORLD);

		if (check_solved(grid, n))
			++tot_solved;

		/* Free the grid */
		free_grid(grid, n);
	}

	/* End timing */
	end_time = clock();
	computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

	fflush(stdout);
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		printf("\nTotal computation completed in %.6f seconds.\n",
		computation_time);

		printf("Sudokus completely solved: %d\n\n", tot_solved);
	}

	/* Free allocated resources */
	fclose(file);
	
	MPI_Finalize();
	return 0;
}
