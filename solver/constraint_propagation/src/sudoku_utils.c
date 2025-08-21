#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../include/sudoku_utils.h"
#include "../include/linked_list.h"
#include "../include/bitmask_utils.h"

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

int check_solved(int **grid, int n)
{
	int i, j;

	for (i = 0; i < n; ++i)
		for (j = 0; j < n; ++j)
			if (grid[i][j] == 0)
				return 0;

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

sudoku_collection_t *create_sudoku_collection(int n, int initial_capacity)
{
	sudoku_collection_t *collection = (sudoku_collection_t *)malloc(sizeof(sudoku_collection_t));
	if (collection == NULL) {
		return NULL;
	}

	collection->grids = (int ***)malloc(initial_capacity * sizeof(int **));
	collection->extended_grids = (struct node ****)malloc(initial_capacity * sizeof(struct node ***));
	if (collection->grids == NULL || collection->extended_grids == NULL) {
		if (collection->grids != NULL) {
			free(collection->grids);
		}
		if (collection->extended_grids != NULL) {
			free(collection->extended_grids);
		}
		free(collection);
		return NULL;
	}

	collection->count = 0;
	collection->n = n;
	collection->capacity = initial_capacity;

	return collection;
}

sudoku_collection_t *read_all_sudokus_from_file(const char *filename, int n)
{
	FILE *file;
	int initial_capacity = 100; /* Start with capacity for 100 grids */
	sudoku_collection_t *collection;
	int **grid;
	struct node *** extended;
	int read_status;

	file = fopen(filename, "r");
	if (!file) {
		fprintf(stderr, "Error: Failed to open file %s\n", filename);
		return NULL;
	}

	collection = create_sudoku_collection(n, initial_capacity);
	if (collection == NULL) {
		fprintf(stderr, "Error: Failed to create sudoku collection\n");
		fclose(file);
		return NULL;
	}

	while (1) {
		grid = create_grid(n);
		if (grid == NULL) {
			fprintf(stderr, "Error: Failed to allocate memory for grid\n");
			free_sudoku_collection(collection);
			fclose(file);
			return NULL;
		}

		read_status = read_grid_from_file(grid, file, n);

		if (read_status != 0) {
			/* If read failed because we reached EOF */
			if (feof(file)) {
				free_grid(grid, n); /* Free the unused grid */
				break;
			} else {
				fprintf(stderr, "Error: Failed to read grid from file\n");
				free_grid(grid, n);
				free_sudoku_collection(collection);
				fclose(file);
				return NULL;
			}
		}

		/* Expand collection if needed */
		if (collection->count >= collection->capacity) {
			int new_capacity = collection->capacity * 2;
			int ***new_grids = (int ***)realloc(collection->grids, 
							    new_capacity * sizeof(int **));
			if (new_grids == NULL) {
				fprintf(stderr, "Error: Failed to expand sudoku collection\n");
				free_grid(grid, n);
				free_sudoku_collection(collection);
				fclose(file);
				return NULL;
			}

			collection->grids = new_grids;
			collection->capacity = new_capacity;
		}

		/* Extend the grid with candidates*/
		extended = extend_grid_with_candidates(grid, n);
		if(extended == NULL){
			fprintf(stderr, "Error: Failed to extend grid with candidates\n");
			free_grid(grid, n);
			free_sudoku_collection(collection);
			fclose(file);
			return NULL;
		}

		/* Add grid to collection */
		collection->grids[collection->count] = grid;
		collection->extended_grids[collection->count] = extended;
		collection->count++;
	}

	fclose(file);
	printf("Successfully read %d Sudoku grids from file.\n", collection->count);
	return collection;
}

void display_sudoku_collection(sudoku_collection_t *collection)
{
	int i; /* Loop variable */

	if (collection == NULL || collection->grids == NULL) {
		printf("No Sudoku grids to display.\n");
		return;
	}

	for (i = 0; i < collection->count; i++) {
		printf("Sudoku Grid %d:\n", i + 1);
		display_sudoku(collection->grids[i], collection->n);
		printf("\n");
	}

	printf("Total Sudoku grids in collection: %d\n", collection->count);
	printf("Size of each grid: %d x %d\n", collection->n, collection->n);
	printf("Current capacity of collection: %d\n", collection->capacity);
}

void free_sudoku_collection(sudoku_collection_t *collection)
{
	int i, r, c;

	if (collection == NULL) {
		return;
	}

	if (collection->grids != NULL) {
		for (i = 0; i < collection->count; i++) {
			if (collection->grids[i] != NULL) {
				free_grid(collection->grids[i], collection->n);
			}
	            	if (collection->extended_grids && collection->extended_grids[i] != NULL) {
                	/* Free each extended grid cell’s linked list */
                	for (r = 0; r < collection->n; r++) {
                    		for (c = 0; c < collection->n; c++) {
                       			free_list(collection->extended_grids[i][r][c]);
                   	 	}
                    		free(collection->extended_grids[i][r]);
                	}
                	free(collection->extended_grids[i]);
			}
		}
		free(collection->grids);
		free(collection->extended_grids);
	}

	free(collection);
}

int **get_grid_from_collection(sudoku_collection_t *collection, int index)
{
	if (collection == NULL || index < 0 || index >= collection->count) {
		return NULL;
	}

	return collection->grids[index];
}

struct node ***get_extended_grid_from_collection(sudoku_collection_t *collection, int index)
{
	if (collection == NULL || index < 0 || index >= collection->count) {
		return NULL;
	}
	return collection->extended_grids[index];
}

void set_extended_grid_in_collection(sudoku_collection_t *collection, int index, struct node ***extended_grid)
{
	collection->extended_grids[index] = extended_grid;
}
