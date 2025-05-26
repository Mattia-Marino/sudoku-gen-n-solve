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
	int i;
	char *filename;
	int n;
	int sqrt_n;
	int tot_solved;
	int **grid;
	FILE *file;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Master variables */
	int num_slaves;
	int terminate_signal;
	int s_rank;
	int **received_grid;
	MPI_Status status;
	int slave_rank;
	int tot_sudoku;
	int n_sudoku_to_send;
	int solved_sudoku_collected;

	/* Slave variables */
	int temp;

	/* MPI variables */
	int rank, size;

	/* Inizialize MPI */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Check if the correct number of arguments is passed */
	if (rank == 0 && argc != 3) {
		fprintf(stderr, "Usage: %s <size> <filename>\n", argv[0]);

		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
		return EXIT_FAILURE;
	}

	/* Read the size of the file from command line */
	n = atoi(argv[1]);

	/* Checks on puzzle size */
	if (rank == 0 && n < 1) {
		fprintf(stderr, "Error: Puzzle size can't be 0 or a negative number\n");
		
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
		return EXIT_FAILURE;
	}

	sqrt_n = (int)sqrt(n);
	if (rank == 0 && sqrt_n * sqrt_n != n) {
		fprintf(stderr, "Error: Size %d must be a perfect square (4, 9, 16, ...)\n", n);
		
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
		return EXIT_FAILURE;
	}

	if (rank == 0) { /* Master process */
		num_slaves = size - 1;
		if (num_slaves == 0) {
			DPRINTF("Fuck\n");

			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Parse the filename from command line */
		filename = argv[2];

		/* Open the file */
		file = fopen(filename, "r");
		if (file == NULL) {
			fprintf(stderr, "Error: Unable to open file %s\n", filename);
			
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Count the lines in the file. One line per sudoku to solve */
		tot_sudoku = count_lines_in_file(file);

		/* Start timing the computation */
		start_time = clock();

		tot_solved = 0;
		/* Master-slave logic */
		n_sudoku_to_send = 0;
		solved_sudoku_collected = 0;

		/* Allocate memory for the Sudoku grid */
		grid = create_grid(n);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			fclose(file);
			
			MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
			return EXIT_FAILURE;
		}

		/* Now, in the variable grid we should have a sudoku, that we want to send to a slave to solve */
		/* Phase 1: Send initial tasks to available slaves */
		for (s_rank = 1; s_rank <= num_slaves && n_sudoku_to_send < tot_sudoku; ++s_rank) {
			/* Read the Sudoku grid from the file */
			read_grid_from_file(grid, file, n);
			
			MPI_Send(&n_sudoku_to_send, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD); /* Say that slave shouldn't be terminated */
			for (i = 0; i < n; ++i)
				MPI_Send(grid[i], n, MPI_INT, s_rank, 1, MPI_COMM_WORLD); /* Send the grid to process */

			n_sudoku_to_send++;
		}

		/* Phase 2: Collect results and distribute remaining tasks (reschedule) */
		received_grid = (int **)malloc(n * sizeof(int *));
		for (i = 0; i < n; ++i)
			received_grid[i] = (int *)malloc(n * sizeof(int));
		
		while (solved_sudoku_collected < tot_sudoku) {
			MPI_Recv(&slave_rank, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
			DPRINTF("Received sudoku n. %d by process %d\n\n", solved_sudoku_collected, slave_rank);
			for (i = 0; i < n; ++i)
				MPI_Recv(received_grid[i], n, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD, &status);

			if (check_solved(received_grid, n))
				++tot_solved;

			DPRINTF("The proposed grid by rank %d:\n", slave_rank);
			DPRINT_SUDOKU(received_grid, n);

			DPRINTF("\n\n--------------------\n\n");
			
			solved_sudoku_collected++;

			if (n_sudoku_to_send < tot_sudoku) {
				read_grid_from_file(grid, file, n);
				MPI_Send(&n_sudoku_to_send, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); /* Say that slave shouldn't be terminated */
				for (i = 0; i < n; ++i)
					MPI_Send(grid[i], n, MPI_INT, status.MPI_SOURCE, 1, MPI_COMM_WORLD);
				
				n_sudoku_to_send++;
			} else {
				/* All sudoku puzzles have been sent out, tell this slave to terminate */
				terminate_signal = -1;
				MPI_Send(&terminate_signal, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
			}
		}

		free_grid(received_grid, n);

		/* Phase 3: Terminate any slaves that were not given any initial work (if num_slaves > n) */
		/* These slaves are waiting for their first message. */
		for (s_rank = 1; s_rank <= num_slaves; ++s_rank) {
			if (s_rank > tot_sudoku) {
				/* These slaves were not sent any work if n < num_slaves */
				terminate_signal = -1;
				MPI_Send(&terminate_signal, 1, MPI_INT, s_rank, 0, MPI_COMM_WORLD);
			}
		}

		free_grid(grid, n);

		/* End timing */
		end_time = clock();
		computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
		printf("\nTotal computation completed in %.6f seconds.\n",
		computation_time);

		printf("Sudokus completely solved: %d\n\n", tot_solved);

		/* Free allocated resources */
		fclose(file);
	} else { /* Slave processes (rank > 0) */
		grid = (int **)malloc(n * sizeof(int *));
		for (i = 0; i < n; ++i)
			grid[i] = (int *)malloc(n * sizeof(int));

		while (1) {
			MPI_Recv(&temp, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status); /* Check if should be terminated */
			if (temp == -1)	/* Termination signal was received*/
				break;
			
			DPRINTF("Process %d has received sudoku n. %d\n", rank, temp);

			for (i = 0; i < n; ++i)
				MPI_Recv(grid[i], n, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);

			/* Solve the sudoku */
			sudoku_solver(grid, n);

			/* Send the rank */
			MPI_Send(&rank, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);

			/* Send the solved sudoku */
			for (i = 0; i < n; ++i)
				MPI_Send(grid[i], n, MPI_INT, 0, 1, MPI_COMM_WORLD); /* Send back solved sudoku */
		}
		free_grid(grid, n);
	}

	MPI_Finalize();
	return 0;
}
