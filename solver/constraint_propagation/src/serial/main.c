/* SPDX-License-Identifier: GPL-3.0 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../../include/solver.h"
#include "../../include/sudoku_utils.h"

int main(int argc, char **argv)
{
	char *filename;
	int i;  /* Loop variable */
	int k;
	int n;
	int **grid;
	FILE *file;
	char *trimmed_line;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
	clock_t start_time;
	clock_t end_time;
	double computation_time;

	/* Check if the correct number of arguments is passed */
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	/* Parse the filename from command line */
	filename = argv[1];

	/* Open the file */
    file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Could not open file %s\n", filename);
        return 1;
    }

    /* Process each line in the file */
    while ((read = getline(&line, &len, file)) != -1) {
		/* Remove newline character if present */
        line[strcspn(line, "\r\n")] = '\0';

        /* Trim the line */
        trimmed_line = malloc(strlen(line) + 1);
        if (trimmed_line == NULL) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            continue;
        }
        k = 0;
        for (i = 0; line[i] != '\0'; i++) {
            if (line[i] != ' ') {
                trimmed_line[k++] = line[i];
            }
        }
        trimmed_line[k] = '\0';

        /* Determine the size of the grid */
        n = (int)sqrt(strlen(trimmed_line));
        if (n * n != strlen(trimmed_line)) {
            fprintf(stderr, "Error: Invalid Sudoku grid format\n");
            continue;
        }

        /* Allocate memory for the Sudoku grid */
        grid = create_grid(n);
        if (grid == NULL) {
            fprintf(stderr, "Error: Failed to allocate memory for grid\n");
            continue;
        }

        /* Read the grid from the line */
        if (read_grid_from_file(grid, line) == -1) {
            fprintf(stderr, "Error: Failed to read grid from line\n");
            free_grid(grid, n);
            continue;
        }

        /* Display the given Sudoku grid */
        printf("\nGiven Sudoku grid:\n");
        display_sudoku(grid, n);
        printf("\n\n\n");

        /* Start timing the computation */
        start_time = clock();

        printf("Solving the sudoku...\n\n");
        sudoku_solver(grid, n);
        printf("The proposed grid:\n");
        display_sudoku(grid, n);

        /* End timing */
        end_time = clock();
        computation_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
        printf("\nTotal computation completed in %.6f seconds.\n",
               computation_time);

        /* Free allocated memory */
        free_grid(grid, n);
    }

    /* Clean up */
    free(line);
    fclose(file);

    return 0;
}
