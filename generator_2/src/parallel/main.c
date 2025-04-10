/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/sudoku.h"
#include "../../include/solver.h"

int main(int argc, char **argv)
{
	int n;
	int num_puzzles;
	int puzzles_per_process;
	int count;
	int sqrt_n;
	int seed;
	struct Sudoku *sudoku;
	char filename[32];
	FILE *file;

	int rank, size;
	double start_time, end_time;

	MPI_Init(&argc, &argv);
	start_time = MPI_Wtime();

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	/* Check if we have the right number of arguments */
	if (argc != 3) {
		printf("Usage: %s <size> <number_of_puzzles>\n", argv[0]);
		return 1;
	}

	/* Parse the size from command line */
	n = atoi(argv[1]);
	num_puzzles = atoi(argv[2]);
	puzzles_per_process = num_puzzles / size;

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
	snprintf(filename, sizeof(filename), "output_%d_%d.%d.txt", n,
		 num_puzzles, rank);
	file = fopen(filename, "w");

	if (file == NULL) {
		fprintf(stderr, "Error: Could not open %s for writing\n",
			filename);
		return 1;
	}

	for (count = 0; count < puzzles_per_process; ++count) {
		printf("Process %d: generating puzzle %d of %d...\n", rank,
		       count + 1, puzzles_per_process);

		/* Initialize the Sudoku grid */
		sudoku = initSudoku(n);

		/* Fill the first row with a random permutation */
		seed = time(NULL) + rank * 1000 + count;
		insertFirstLine(sudoku, seed);

		/* Generate a complete Sudoku grid */
		SudokuSolver(sudoku);

		/* Remove numbers to generate a playable puzzle */
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

	MPI_Barrier(MPI_COMM_WORLD);
	if (rank == 0) {
		int num_proc;
		char filename_proc[64];
		char filename_final[64];
		char buffer[1024];
		FILE *file_proc;
		FILE *file_final;
		size_t bytes_read;

		snprintf(filename_final, sizeof(filename_final),
			 "output_%d_%d.txt", n, num_puzzles);
		file_final = fopen(filename_final, "w");

		if (file_final == NULL) {
			fprintf(stderr,
				"Error: Could not open %s for writing\n",
				filename_final);
			return 1;
		}

		printf("\n\nMerging all puzzles into %s...\n", filename_final);

		for (num_proc = 0; num_proc < size; ++num_proc) {
			snprintf(filename_proc, sizeof(filename_proc),
				 "output_%d_%d.%d.txt", n, num_puzzles,
				 num_proc);

			printf("\tCopying %s...\n", filename_proc);
			file_proc = fopen(filename_proc, "r");

			if (file_proc == NULL) {
				fprintf(stderr,
					"Error: Could not open %s for reading\n",
					filename_proc);
				return 1;
			}

			while ((bytes_read = fread(buffer, 1, 1024, file_proc)) > 0) {
				if (fwrite(buffer, 1, bytes_read,
					   file_final) != bytes_read) {
					perror("Error writing to output file");
					fclose(file_proc);
					fclose(file_final);
					return 1;
				}
			}

			if (ferror(file_proc)) {
				perror("Error reading from input file");
				fclose(file_proc);
				fclose(file_final);
				return 1;
			}

			fclose(file_proc);
			remove(filename_proc);
			printf("\tCopying %s done.\n\n", filename_proc);
		}

		fclose(file_final);
		printf("All puzzles saved to %s\n", filename_final);
	}

	end_time = MPI_Wtime();
	if (rank == 0) {
		printf("Total time taken: %f seconds\n", end_time - start_time);
	}

	MPI_Finalize();
	return 0;
}