/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <mpi.h>

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
	double start_time;
	double end_time;
	double computation_time;

	
	/* Initialize MPI */
	MPI_Init(&argc, &argv);

	MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);

	int myrank, size;
    	MPI_Comm_size(MPI_COMM_WORLD, &size);
    	MPI_Comm_rank(MPI_COMM_WORLD, &myrank);


	/* Check if the correct number of arguments is passed */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);
		MPI_Finalize();
		return 1;
	}

	/* Read the size of the file from command line */
	n = atoi(argv[1]);

	/* Checks on puzzle size */
	if (n < 1) {
		fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
		MPI_Finalize();
		return 1;
	}

	sqrt_n = (int)sqrt(n);
	if (sqrt_n * sqrt_n != n) {
		fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
		MPI_Finalize();
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[2];

	/* Open the file */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		MPI_Finalize();
		return 1;
	}

	start_time = MPI_Wtime();
	tot_solved = 0;
	while(1){
		/* Allocate grid*/
		grid = create_grid(n);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			fclose(file);
			MPI_Finalize();
			return 1;
		}

		/* Read the grid from the file*/
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
				MPI_Finalize();
				return 1;
			}
		}
	
		/*
		MPI_Bcast(&grid[0][0], n * n, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
		*/

		MPI_Barrier(MPI_COMM_WORLD);

		if (myrank == 0) {
			/* Display the given Sudoku grid */
			DPRINTF("\nI am process %d. Given Sudoku grid:\n", myrank);
			DPRINT_SUDOKU(grid, n);
			DPRINTF("\n\n---------------------\n\n");
		}

		MPI_Barrier(MPI_COMM_WORLD);

		if (myrank == 1) {
			/* Display the given Sudoku grid */
			DPRINTF("\nI am process %d. Given Sudoku grid:\n", myrank);
			DPRINT_SUDOKU(grid, n);
			DPRINTF("\n\n---------------------\n\n");
		}

		MPI_Barrier(MPI_COMM_WORLD);

		sudoku_solver_parallel(grid, n, myrank, size);

		DPRINTF("Process %d waiting at barrier...\n\n", myrank);
		fflush(stdout);

		MPI_Barrier(MPI_COMM_WORLD);

		DPRINTF("We are all past the barrier - %d\n", myrank);
		fflush(stdout);

		if (grid == NULL) {
			DPRINTF("Error: Grid is NULL after solving\n");
			fprintf(stderr, "Error: Grid is NULL after solving\n");
			MPI_Finalize();
			return 1;
		}

		/* Master process prints the computation time */
		if (myrank == 0) {
			DPRINTF("The proposed grid:\n");
			DPRINT_SUDOKU(grid, n);
	
			DPRINTF("\n\n--------------------\n\n");

			if (check_solved(grid, n))
				++tot_solved;
		}

		/* Free the grid after solving */
                free_grid(grid, n);
		/* Set the grid to NULL to prevent accidental double-free*/
		grid = NULL;  

		MPI_Barrier(MPI_COMM_WORLD);
	}

	/* Free the grid after the loop if it hasn't been freed */
	if (grid != NULL) {
   	        free_grid(grid, n);
    		grid = NULL;
	}

	DPRINTF("Process %d - Out of the loop\n", myrank);

	end_time = MPI_Wtime();
	computation_time = end_time - start_time;
	if (myrank == 0) {
		fprintf(stdout, "Total computation time: %f seconds\n", computation_time);
		fprintf(stdout, "Total solved: %d\n", tot_solved);
	}	

	DPRINTF("Closing the file - %d\n", myrank);
	fclose(file);

	DPRINTF("I have finished everything - %d\n", myrank);

	MPI_Finalize();
	return 0;
}