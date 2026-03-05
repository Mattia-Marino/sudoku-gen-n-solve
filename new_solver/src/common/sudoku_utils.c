#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../include/sudoku_utils.h"

int **create_grid(int n)
{
	int i, j; /* Loop variables */
	int **grid;

	grid = (int **)malloc(n * sizeof(int *));
	for (i = 0; i < n; i++) {
		grid[i] = (int *)malloc(n * sizeof(int));
		for (j = 0; j < n; j++) {
			grid[i][j] = 0;
		}
	}
	return grid;
}

void free_grid(int **grid, int n)
{
	int i; /* Loop variable */

	for (i = 0; i < n; i++) {
		free(grid[i]);
	}
	free(grid);
}

int read_grid_from_file(int **grid, FILE *file, int n)
{
	int i, j;

	/* Read values and fill the grid */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (fscanf(file, "%d", &grid[i][j]) != 1) {
				if (feof(file))
					return 1;

				fprintf(stderr,
					"Error: Invalid grid data at position [%d][%d]\n",
					i, j);
				return -1;
			}
		}
	}

	return 0;
}

int read_grid_from_string(int **grid, const char *line, int n)
{
	int i, j;
	const char *ptr;
	char *endptr;
	long value;

	if (line == NULL || grid == NULL) {
		fprintf(stderr, "Error: NULL pointer passed to read_grid_from_string\n");
		return -1;
	}

	ptr = line;

	/* Read values and fill the grid */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			/* Skip leading whitespace */
			while (*ptr == ' ' || *ptr == '\t')
				ptr++;

			/* Check if we've reached end of string prematurely */
			if (*ptr == '\0' || *ptr == '\n') {
				fprintf(stderr,
					"Error: Insufficient data in string at position [%d][%d]\n",
					i, j);
				return -1;
			}

			/* Parse the integer */
			value = strtol(ptr, &endptr, 10);

			/* Check if parsing was successful */
			if (ptr == endptr) {
				fprintf(stderr,
					"Error: Invalid grid data at position [%d][%d]\n",
					i, j);
				return -1;
			}

			grid[i][j] = (int)value;
			ptr = endptr;
		}
	}

	return 0;
}

int write_grid_to_string(int **grid, char *line, int n)
{
	int i, j;
	char *ptr;
	int written;

	if (line == NULL || grid == NULL) {
		fprintf(stderr, "Error: NULL pointer passed to write_grid_to_string\n");
		return -1;
	}

	ptr = line;

	/* Write grid values to string */
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			written = sprintf(ptr, "%d ", grid[i][j]);
			if (written < 0) {
				fprintf(stderr, "Error: Failed to write grid data at position [%d][%d]\n", i, j);
				return -1;
			}
			ptr += written;
		}
	}

	/* Add newline at the end */
	*ptr = '\n';
	ptr++;
	*ptr = '\0';

	return 0;
}

size_t get_grid_string_size(int n) {
	int max_val = n;
	int digits_per_number = 0;

	/* Calculate number of digits in n (the largest possible value) */
	if (max_val == 0) {
		digits_per_number = 1;
	} else {
		int temp = max_val;
		while (temp > 0) {
			temp /= 10;
			digits_per_number++;
		}
	}

	/* * Formula:
	* (Total Cells * (Digits + Space)) + Newline + NullTerminator
	*/
	size_t total_cells = (size_t)n * n;
	size_t chars_per_cell = digits_per_number + 1; // digit(s) + ' '
	
	return (total_cells * chars_per_cell) + 1 + 1; 
}

int check_solved(int **grid, int n)
{
	int i, j;

	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j)
			if (grid[i][j] == 0)
				return 0;

	return 1;
}

int check_solved_string(const char *line, int n)
{
	const char *ptr;
	char *endptr;
	long value;
	int count;

	if (line == NULL) {
		fprintf(stderr, "Error: NULL pointer passed to check_solved_string\n");
		return -1;
	}

	ptr = line;
	count = 0;

	/* Parse all numbers in the string */
	while (*ptr != '\0' && *ptr != '\n' && count < n * n) {
		/* Skip leading whitespace */
		while (*ptr == ' ' || *ptr == '\t')
			ptr++;

		/* Check if we've reached end of string */
		if (*ptr == '\0' || *ptr == '\n')
			break;

		/* Parse the integer */
		value = strtol(ptr, &endptr, 10);

		/* Check if parsing was successful */
		if (ptr == endptr)
			break;

		/* If we find a 0, the sudoku is not solved */
		if (value == 0)
			return 0;

		ptr = endptr;
		count++;
	}

	return 1;
}

void display_sudoku(int **grid, int n)
{
	int i, j; /* Loop variables */
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
			if (grid[i][j] == 0) {
				printf(" . ");
			} else {
				printf("%2d ", grid[i][j]);
			}
		}
		printf("\n");
	}
}

int count_lines_in_file(FILE *file) {
	if (file == NULL) {
		fprintf(stderr, "Error: Invalid file pointer (NULL) passed to countLinesAndReset_with_FILE_ptr.\n");
		return -1; /* Indicate an error */
	}

	int lineCount = 0;
	int ch;

	/* Reset to the beginning of the file to ensure we count from the start */
	rewind(file);

	/* Read character by character and count newlines */
	while ((ch = fgetc(file)) != EOF) {
		if (ch == '\n') {
		lineCount++;
		}
	}

	/* Handle the case where the last line doesn't end with a newline */
	if (ftell(file) > 0) {
		/* Go back one character to check the very last character if it wasn't EOF right away */
		fseek(file, -1, SEEK_END); /* Move to the last character */
		ch = fgetc(file); /* Read the last character */

		if (ch != '\n') {
		lineCount++; /* Increment if the last character is not a newline */
		}
	}


	/* Rewind the file pointer to the beginning again for subsequent processing */
	rewind(file);

	return lineCount;
}