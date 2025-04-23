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

int read_grid_from_file(int **grid, const char *line)
{
	int l, i, j; 	    /* Loop variables */
	int k; 		        /* Index for trimmed line */
	char *trimmed_line; /* Pointer to the trimmed line */
	int n; 		        /* Size of the grid */

    /* Remove spaces from the line */
    trimmed_line = malloc(strlen(line) + 1);
    k = 0;
    for (l = 0; line[l] != '\0'; l++) {
        if (line[l] != ' ') {
            trimmed_line[k++] = line[l];
        }
    }
    trimmed_line[k] = '\0';

    /* Calculate the size of the grid */
    n = (int)sqrt(strlen(trimmed_line));
    if (n * n != strlen(trimmed_line)) {
        fprintf(stderr, "Error: Invalid Sudoku grid format\n");
        free(trimmed_line);
        return -1;
    }

    /* Populate the grid */
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) {
            grid[i][j] = trimmed_line[i * n + j] - '0'; 
            if (grid[i][j] < 0 || grid[i][j] > 9) {
                fprintf(stderr, "Error: Invalid character in Sudoku grid\n");
                free(trimmed_line);
                return -1;
            }
        }
    }

    free(trimmed_line);
    return n; 
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