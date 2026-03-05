/* SPDX-License-Identifier: GPL-3.0 */

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <time.h>
#include <string.h>

#include "../../include/debug.h"
#include "../../include/solver.h"
#include "../../include/sudoku_utils.h"

int main(int argc, char **argv)
{
	/* Parsing and Processing */
	char *filename = NULL;
	FILE *file;
	int opt;
	int sudoku_size;
	int **grid;
	int read_status;

	/* Post-processing */
	int tot_solved;

	/* Timing */
	struct timespec start_time, end_time;
	double computation_time;

	/* Default sudoku size */
	sudoku_size = 9;

	/* ******************************************************************
	* ALL RANKS: ARGUMENT PARSING
	* All MPI processes parse command-line arguments independently,
	* eliminating the need to broadcast configuration values.
	* ******************************************************************/
	{
		int sqrt_n;

		static struct option long_options[] = {
			{"nthreads", required_argument, 0, 'n'},
			{"size",     required_argument, 0, 's'},
			{0, 0, 0, 0}
		};

		while ((opt = getopt_long(argc, argv, "s:", long_options, NULL)) != -1) {
			switch (opt) {
			case 's': sudoku_size = atoi(optarg); break;
			default:
				return EXIT_FAILURE;
			}
		}

		if (optind < argc) {
			filename = argv[optind];
		} else {
			fprintf(stderr, "Error: Missing filename\n");
			return EXIT_FAILURE;
		}

		/* Basic Validation */
		if (sudoku_size < 1)
			return EXIT_FAILURE;

		sqrt_n = (int)sqrt(sudoku_size);
		if (sqrt_n * sqrt_n != sudoku_size)
			return EXIT_FAILURE;
	}

	/* Open the file */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return 1;
	}

	/* Start timing the computation */
	clock_gettime(CLOCK_MONOTONIC, &start_time);

	tot_solved = 0;
	while (1) {
		/* Allocate memory for the Sudoku grid */
		grid = create_grid(sudoku_size);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			fclose(file);
			return 1;
		}

		/* Read the Sudoku grid from the file */
		read_status = read_grid_from_file(grid, file, sudoku_size);

		if (read_status != 0) {
			/* If read failed because we reached EOF */
			if (feof(file)) {
				DPRINTF("Reached EOF\n");
				break;
			} else {
				fprintf(stderr, "Error: Failed to read grid from file\n");
				free_grid(grid, sudoku_size);
				fclose(file);
				return 1;
			}
		}

		/* Display the given Sudoku grid */
		DPRINTF("\nGiven Sudoku grid:\n");
		DPRINT_SUDOKU(grid, sudoku_size);
		DPRINTF("\n\n\n");

		/* Solve the sudoku */
		DPRINTF("Solving the sudoku...\n\n");
		sudoku_solver(grid, sudoku_size);

		DPRINTF("The proposed grid:\n");
		DPRINT_SUDOKU(grid, sudoku_size);

		DPRINTF("\n\n--------------------\n\n");

		if (check_solved(grid, sudoku_size))
			++tot_solved;

		/* Free the grid */
		free_grid(grid, sudoku_size);
	}

	/* End timing */
	clock_gettime(CLOCK_MONOTONIC, &end_time);
	computation_time = (end_time.tv_sec - start_time.tv_sec) +
					(end_time.tv_nsec - start_time.tv_nsec) / 1e9;
	printf("\nTotal computation completed in %.6f seconds.\n", computation_time);

	printf("Sudokus completely solved: %d\n\n", tot_solved);

	/* Free allocated resources */
	fclose(file);
	return 0;
}
