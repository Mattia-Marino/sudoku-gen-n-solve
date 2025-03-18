/* SPDX-License-Identifier: GPL-3.0 */

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
	int i;				/* Loop counter */
	struct Sudoku *sudoku;		/* Pointer to the Sudoku struct */

	/* Allocating memory for the Sudoku struct */
	sudoku = malloc(sizeof(struct Sudoku));
	assert(sudoku);

	/* Initializing the fields of the Sudoku struct */
	sudoku->size = size;
	sudoku->squareRootOfSize = (int)sqrt(size);

	/* Allocating memory for the Sudoku grid and initializing it to zero */
	sudoku->grid = malloc(size * sizeof(int *));
	assert(sudoku->grid);

	for (i = 0; i < size; i++) {
		sudoku->grid[i] = malloc(size * sizeof(int));
		memset(sudoku->grid[i], 0, size * sizeof(int));
	}

	return sudoku;
}

/* Function for displaying the Sudoku grid */
void displaySudoku(struct Sudoku *sudoku)
{
	int i, j;			/* Loop counters */
	int size, block_size;		/* Size of the Sudoku grid and block size */

	size = sudoku->size;
	block_size = sudoku->squareRootOfSize;

	for (i = 0; i < size; i++) {
		/* Print horizontal line */
		if (i % block_size == 0 && i != 0) {
			for (j = 0; j < size; j++) {
				printf("---");
				if ((j + 1) % block_size == 0 && (j + 1) < size)
					printf("+");
			}
			printf("\n");
		}

		for (j = 0; j < size; j++) {
			/* Print vertical line */
			if (j % block_size == 0 && j != 0)
				printf("|");

			/* Print the cell content or a dot for empty cells */
			if (sudoku->grid[i][j] == 0)
				printf(" . ");
			else
				printf("%2d ", sudoku->grid[i][j]);
		}
		printf("\n");
	}
}

/* Create a new grid with the given size */
int **createGrid(int n)
{
	int i, j;
	int **grid;

	grid = (int **)malloc(n * sizeof(int *));

	for (i = 0; i < n; i++) {
		grid[i] = (int *)malloc(n * sizeof(int));
		for (j = 0; j < n; j++)
			grid[i][j] = 0;
	}
	return grid;
}

/* Free the allocated memory for the grid */
void freeGrid(int **grid, int n)
{
	int i;

	for (i = 0; i < n; i++)
		free(grid[i]);

	free(grid);
}

/* Print the Sudoku grid */
void printGrid(int **grid, int n)
{
	int i, j;
	int sqrt_n;

	sqrt_n = (int)sqrt(n);
	for (i = 0; i < n; i++) {
		/* Print horizontal line */
		if (i % sqrt_n == 0 && i != 0) {
			for (j = 0; j < n; j++) {
				printf("---");
				if ((j + 1) % sqrt_n == 0 && (j + 1) < n)
					printf("+");
			}
			printf("\n");
		}

		for (j = 0; j < n; j++) {
			/* Print vertical line */
			if (j % sqrt_n == 0 && j != 0)
				printf("|");

			/* Print the cell content or a dot for empty cells */
			if (grid[i][j] == 0)
				printf(" . ");
			else
				printf("%2d ", grid[i][j]);
		}
		printf("\n");
	}
}

int readSizeFromFile(const char *filename)
{
	FILE *file;	/* File pointer */
	int n;		/* Size of the Sudoku grid */
	int sqrt_n;	/* Square root of n */

	/* Open the file for reading */
	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		return 0;
	}

	/* Read grid size from first line */
	if (fscanf(file, "%d", &n) != 1) {
		fprintf(stderr, "Error: Invalid file format\n");
		fclose(file);
		return 0;
	}

	/* Check if n is a positive number */
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		fclose(file);
		return 0;
	}

	/* Calculate square root of n */
	sqrt_n = (int)sqrt(n);

	/* For Sudoku, n MUST be a perfect square (4, 9, 16, etc.) */
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		fclose(file);
		return 0;
	}

	fclose(file);
	return n;
}

/* Reads a Sudoku grid from a file */
void readGridFromFile(struct Sudoku *sudoku, const char *filename)
{
	FILE *file;
	int n;
	int sqrt_n;
	int **grid;
	int i, j;

	file = fopen(filename, "r");
	if (file == NULL) {
		fprintf(stderr, "Error: Unable to open file %s\n", filename);
		/* return NULL; */
	}

	/* Read grid size from first line */
	if (fscanf(file, "%d", &n) != 1) {
		fprintf(stderr, "Error: Invalid file format\n");
		fclose(file);
		/* return NULL; */
	}

	/* Check if n is a positive number */
	if (n <= 0) {
		printf("Size must be a positive integer.\n");
		fclose(file);
		/* return NULL; */
	}

	/* Calculate square root of n */
	sqrt_n = (int)sqrt(n);

	/* For Sudoku, n MUST be a perfect square (4, 9, 16, etc.) */
	if (sqrt_n * sqrt_n != n) {
		printf("Error: Size %d is not a perfect square.\n", n);
		printf("For Sudoku, please provide a perfect square number like 4, 9, 16, etc.\n");
		fclose(file);
		/* return NULL; */
	}

	/* Create grid with the appropriate size */
	grid = createGrid(n);

	if (grid == NULL) {
		fprintf(stderr, "Error: Failed to allocate memory for grid\n");
		fclose(file);
		/* return NULL; */
	}

	/* Read values and fill the grid */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (fscanf(file, "%d", &grid[i][j]) != 1) {
				fprintf(stderr,
					"Error: Invalid grid data at position [%d][%d]\n",
					i, j);
				freeGrid(grid, n);
				fclose(file);
				/* return NULL; */
			}
		}
	}

	/* Close the file */
	fclose(file);

	/* Assign the grid to the Sudoku struct */
	sudoku->grid = grid;
}

/* Function for deallocating the memory of a Sudoku */
void destroySudoku(struct Sudoku *sudoku)
{
	int i;

	if (sudoku == NULL)
		return;

	if (sudoku->grid != NULL) {
		for (i = 0; i < sudoku->size; i++)
			free(sudoku->grid[i]);

		free(sudoku->grid);
	}
	free(sudoku);
}
