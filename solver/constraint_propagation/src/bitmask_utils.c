#include <stdio.h>
#include <stdlib.h>

#include "../include/bitmask_utils.h"
#include "../include/linked_list.h"
#include "../include/sudoku_utils.h"

struct node*** extend_grid_with_candidates(int **grid, int n) {
	int i, j, k;
	struct node ***extended = malloc(n * sizeof(struct node **));
	if (!extended) return NULL;

	for (i = 0; i < n; i++) {
		extended[i] = malloc(n * sizeof(struct node *));
		if (!extended[i]) return NULL;

		for (j = 0; j < n; j++) {
			if (grid[i][j] == 0) {
				/* Fill empty cell with candidates */
				extended[i][j] = NULL;
				for (k = 1; k <= n; k++) {
				extended[i][j] = append(extended[i][j], k);
				}
			} else {
				/* Pre-filled → single candidate */
				extended[i][j] = create_node(grid[i][j]);
			}
		}
	}
	return extended;
}

int** convert_extended_to_bitmask_grid(struct node ***extended_grid, int n) {
	int i, j;
	int **bitmask_grid = malloc(n * sizeof(int *));
	if (!bitmask_grid) return NULL;

	for (i = 0; i < n; i++) {
		bitmask_grid[i] = malloc(n * sizeof(int));
		if (!bitmask_grid[i]) return NULL;

		for (j = 0; j < n; j++) {
			struct node *curr = extended_grid[i][j];
			int mask = 0;
			while (curr != NULL) {
				int val = curr->data;   
				mask |= (1 << (val - 1));
				curr = curr->next;
			}
			bitmask_grid[i][j] = mask;
		}
	}
	return bitmask_grid;
}

struct node*** convert_bitmask_to_extended_grid(int **bitmask_grid, int n) {
	int i, j, k;

	struct node ***extended = malloc(n * sizeof(struct node **));
	if (!extended) return NULL;

	for (i = 0; i < n; i++) {
		extended[i] = malloc(n * sizeof(struct node *));
		if (!extended[i]) return NULL;

		for (j = 0; j < n; j++) {
			int mask = bitmask_grid[i][j];
			extended[i][j] = NULL;

			for (k = 1; k <= n; k++) {
				if (mask & (1 << (k - 1))) 
					extended[i][j] = append(extended[i][j], k);
			}
		}
	}
	return extended;
}

int check_solved_extended(struct node ***extended_grid, int n) {
	int i, j;
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++) {
			if (size_list(extended_grid[i][j]) != 1) {
				/* Unsolved */
				return 0; 
			}
		}
	}
	/* Solved */
	return 1; 
}