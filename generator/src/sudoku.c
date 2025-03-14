// SPDX-License-Identifier: GPL-3.0

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/solver.h"
#include "../include/sudoku.h"

/* Function for initializing the Sudoku struct */
struct Sudoku *initSudoku(int size)
{
	// Allocating memory for the Sudoku struct
	struct Sudoku *sudoku = malloc(sizeof(struct Sudoku));

	assert(sudoku);

	// Initializing the fields of the Sudoku struct
	sudoku->size = size;
	sudoku->squareRootOfSize = (int)sqrt(size);

	// Allocating memory for the Sudoku grid and initializing it to zero
	sudoku->grid = malloc(size * sizeof(int *));
	assert(sudoku->grid);
	for (int i = 0; i < size; i++) {
		sudoku->grid[i] = malloc(size * sizeof(int));
		memset(sudoku->grid[i], 0, size * sizeof(int));
	}

	return sudoku;
}

/* Function for displaying the Sudoku grid */
void displaySudoku(struct Sudoku *sudoku)
{
	int size = sudoku->size, block_size = sudoku->squareRootOfSize;

	for (int i = 0; i < size; i++) {
		// Print horizontal line
		if (i % block_size == 0 && i != 0) {
			for (int j = 0; j < size; j++) {
				printf("---");
				if ((j + 1) % block_size == 0 && (j + 1) < size)
					printf("+");
			}
			printf("\n");
		}

		for (int j = 0; j < size; j++) {
			// Print vertical line
			if (j % block_size == 0 && j != 0)
				printf("|");

			// Print the cell content or a dot for empty cells
			if (sudoku->grid[i][j] == 0)
				printf(" . ");
			else
				printf("%2d ", sudoku->grid[i][j]);
		}
		printf("\n");
	}
}

/* Function to automatically fill the first row of the sudoku for randomization */
void insertFirstLine(struct Sudoku *sudoku)
{
	// Add randomization by filling the first row with a random permutation
	int *firstRow = malloc(sudoku->size * sizeof(int));

	// Initialize the array with consecutive numbers from 1 to size
	for (int i = 0; i < sudoku->size; i++)
		firstRow[i] = i + 1;

	// Shuffle the array using Fisher-Yates algorithm
	srand(time(NULL)); // Initialize random seed
	for (int i = sudoku->size - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		// Swap firstRow[i] and firstRow[j]
		int temp = firstRow[i];

		firstRow[i] = firstRow[j];
		firstRow[j] = temp;
	}

	// Assign the random permutation to the first row
	for (int i = 0; i < sudoku->size; i++)
		sudoku->grid[0][i] = firstRow[i];

	// Free the temporary array
	free(firstRow);
}

/* Function to check if a Sudoku puzzle has a unique solution */
int hasUniqueSolution(struct Sudoku *sudoku, struct Sudoku *solution)
{
	// Create a copy of the current puzzle state
	struct Sudoku *copy = initSudoku(sudoku->size);

	for (int i = 0; i < sudoku->size; i++)
		for (int j = 0; j < sudoku->size; j++)
			copy->grid[i][j] = sudoku->grid[i][j];

	// Try to solve the copy
	int solved = SudokuSolver(copy);

	if (!solved) {
		destroySudoku(copy);
		return 0; // No solution found
	}

	// Check if the solution is different from our original solution
	for (int i = 0; i < sudoku->size; i++) {
		for (int j = 0; j < sudoku->size; j++) {
			if (copy->grid[i][j] != solution->grid[i][j]) {
				destroySudoku(copy);
				return 0; // Different solution found, not unique
			}
		}
	}

	destroySudoku(copy);
	return 1; // Solution is unique
}

/* Function to remove numbers from the board while ensuring a unique solution */
void removeNumbers(struct Sudoku *sudoku)
{
	int size = sudoku->size;
	time_t rawtime;
	struct tm *timeinfo;

	// Create a copy of the complete solution
	struct Sudoku *solution = initSudoku(size);

	for (int i = 0; i < size; i++)
		for (int j = 0; j < size; j++)
			solution->grid[i][j] = sudoku->grid[i][j];

	// Create an array to track essential cells (cells that must keep their values)
	int total_cells = size * size;
	char *essential =
		calloc(total_cells, sizeof(char)); // initialized to 0 (false)

	// Create an array of all cell positions
	struct {
		int row;
		int col;
	} *cells = malloc(total_cells * sizeof(*cells));

	// Initialize the array with all cell positions
	int idx = 0;

	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			cells[idx].row = i;
			cells[idx].col = j;
			idx++;
		}
	}

	// Shuffle the array using Fisher-Yates algorithm
	srand(time(NULL));
	for (int i = total_cells - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		// Swap cells[i] and cells[j]
		int tmp_row = cells[i].row;
		int tmp_col = cells[i].col;

		cells[i].row = cells[j].row;
		cells[i].col = cells[j].col;
		cells[j].row = tmp_row;
		cells[j].col = tmp_col;
	}

	// Try to remove numbers while maintaining a unique solution
	int removed = 0;
	int attempted = 0;

	for (int i = 0; i < total_cells; i++) {
		int row = cells[i].row;
		int col = cells[i].col;

		// Skip if this cell has already been determined to be essential
		if (essential[row * size + col])
			continue;

		int temp = sudoku->grid[row][col];

		attempted++;

		// Try removing this number
		sudoku->grid[row][col] = 0;

		// Check if the puzzle still has a unique solution
		if (!hasUniqueSolution(sudoku, solution)) {
			// If not, put the number back and mark as essential
			sudoku->grid[row][col] = temp;
			essential[row * size + col] = 1;
		} else {
			removed++;
		}

		// Optional: Add a limit to the number of cells to remove for time efficiency
		if (size >= 25 && removed >=
		    total_cells * 0.52) // Remove only about 52% of cells for 25x25 and bigger
			break;

		// // Another optimization: if we've tried a lot without success, quit early
		// if (attempted - removed > total_cells * 0.2)
		//	break;

		// Optional: Print progress
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		printf("Attempt %d - Removed %d - %s", attempted, removed, asctime(timeinfo));
	}

	destroySudoku(solution);
	free(cells);
	free(essential);

	printf("Removed %d/%d numbers from the Sudoku grid.\n", removed,
	       total_cells);
}

/* Function for saving the Sudoku grid to a file in a machine-readable format */
void saveSudokuToFile(struct Sudoku *sudoku)
{
	// Create a dynamic filename based on board size
	char filename[32]; // Buffer for the filename

	snprintf(filename, sizeof(filename), "output_%d.txt", sudoku->size);

	FILE *file = fopen(filename, "w");

	if (file == NULL) {
		fprintf(stderr, "Error: Could not open %s for writing\n",
			filename);
		return;
	}

	// Write size information at the top of file
	fprintf(file, "%d\n", sudoku->size);

	// Write grid data without separators, one row per line
	for (int i = 0; i < sudoku->size; i++) {
		for (int j = 0; j < sudoku->size; j++) {
			// Write each number followed by a space
			fprintf(file, "%d ", sudoku->grid[i][j]);
		}
		fprintf(file, "\n"); // End of row
	}

	fclose(file);
	printf("Sudoku saved to %s\n", filename);
}

/* Function for deallocating the memory of a Sudoku */
void destroySudoku(struct Sudoku *sudoku)
{
	if (sudoku == NULL)
		return;

	if (sudoku->grid != NULL) {
		for (int i = 0; i < sudoku->size; i++)
			free(sudoku->grid[i]);

		free(sudoku->grid);
	}
	free(sudoku);
}
