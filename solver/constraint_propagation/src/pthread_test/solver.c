#define _POSIX_C_SOURCE 200112L

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/debug.h"
#include "../../include/linked_list.h"
#include "../../include/solver.h"
#include "../../include/solver_parallel2.h"
#include "../../include/sudoku_utils.h"

/* ==================================================================
 *  Annotation helpers
 *
 *  Instead of modifying linked lists directly (which would race),
 *  each thread records "delete value V from cell [r][c]" as a bit in
 *  a shared int array:  deletions[r * n + c] |= (1 << V).
 *  __sync_fetch_and_or makes concurrent annotations lock-free.
 * ================================================================== */

static void annotate_row(int *deletions, int n,
			 struct coordinates *coord, int n_coords, int value)
{
	int row = coord[0].row;
	int i, j, skip;

	DPRINTF("\nAnnotating value %d on row %d\n", value, row + 1);

	for (i = 0; i < n; ++i) {
		skip = 0;
		for (j = 0; j < n_coords; ++j)
			if (i == coord[j].column) { skip = 1; break; }
		if (!skip)
			__sync_fetch_and_or(&deletions[row * n + i], 1 << value);
	}
}

static void annotate_column(int *deletions, int n,
			    struct coordinates *coord, int n_coords, int value)
{
	int col = coord[0].column;
	int i, j, skip;

	DPRINTF("\nAnnotating value %d on column %d\n", value, col + 1);

	for (i = 0; i < n; ++i) {
		skip = 0;
		for (j = 0; j < n_coords; ++j)
			if (i == coord[j].row) { skip = 1; break; }
		if (!skip)
			__sync_fetch_and_or(&deletions[i * n + col], 1 << value);
	}
}

static void annotate_box(int *deletions, int n,
			 struct coordinates *coord, int n_coords, int value)
{
	int i, j, k, skip;
	int sqrt_n = (int)sqrt(n);
	int row_start = (coord[0].row / sqrt_n) * sqrt_n;
	int col_start = (coord[0].column / sqrt_n) * sqrt_n;

	DPRINTF("\nAnnotating value %d in box (%d,%d)\n", value,
		row_start / sqrt_n + 1, col_start / sqrt_n + 1);

	for (i = row_start; i < row_start + sqrt_n; ++i) {
		for (j = col_start; j < col_start + sqrt_n; ++j) {
			skip = 0;
			for (k = 0; k < n_coords; ++k)
				if (i == coord[k].row && j == coord[k].column)
					{ skip = 1; break; }
			if (!skip)
				__sync_fetch_and_or(&deletions[i * n + j],
						    1 << value);
		}
	}
}

/* Apply all pending deletions for rows [start_row, end_row) and
 * zero the processed entries so the buffer is clean for the next depth.
 * Returns 1 if at least one deletion was performed. */
static int apply_deletions(struct node ***extended_grid, int *deletions,
			   int n, int start_row, int end_row)
{
	int i, j, v;
	int changed = 0;

	for (i = start_row; i < end_row; ++i) {
		for (j = 0; j < n; ++j) {
			int mask = deletions[i * n + j];
			if (mask) {
				for (v = 1; v <= n; ++v) {
					if (mask & (1 << v))
						extended_grid[i][j] =
							delete_at_given_value(
								extended_grid[i][j], v);
				}
				deletions[i * n + j] = 0;
				changed = 1;
			}
		}
	}
	return changed;
}

/* Allocate a 3-D propagation-tracking array (max_depth x n x n). */
static int ***alloc_prop(int max_depth, int n)
{
	int i, j;
	int ***p = (int ***)malloc(max_depth * sizeof(int **));

	for (i = 0; i < max_depth; ++i) {
		p[i] = (int **)malloc(n * sizeof(int *));
		for (j = 0; j < n; ++j)
			p[i][j] = (int *)malloc(n * sizeof(int));
		initialize_propagation_matrix(p[i], n);
	}
	return p;
}

/* ==================================================================
 *  Parallel solver - annotate-then-delete approach
 *
 *  Thread 1 analyses rows, thread 2 columns, thread 3 boxes.
 *  All three read the grid concurrently (safe - read-only) and
 *  record intended deletions via atomic OR into a shared bitmask.
 *  After a barrier the deletions are bulk-applied (each thread
 *  handles a disjoint row slice), then another barrier makes the
 *  updated grid visible to all before the next depth / iteration.
 *
 *  Three separate already_propagated matrices are used (one per
 *  propagation type) to match the serial solver's behaviour.
 * ================================================================== */
int parallel_sudoku_solver(struct node ***extended_grid, int n,
			   int thread_id, int tg_size,
			   pthread_barrier_t *barrier,
			   volatile int *thread_changed,
			   int *deletions)
{
	int i, j, t;
	int depth, max_depth;
	int numbers_left;
	int local_is_changed;
	int should_continue;
	int apply_start, apply_end;

	/* One tracking matrix per propagation type (matches serial solver) */
	int ***ap_rows, ***ap_cols, ***ap_boxes;

	max_depth = (int)floor((double)n / 2);
	DPRINTF("Thread %d: max_depth = %d\n", thread_id, max_depth);

	ap_rows  = alloc_prop(max_depth, n);
	ap_cols  = alloc_prop(max_depth, n);
	ap_boxes = alloc_prop(max_depth, n);

	/* Row range for the apply phase (disjoint across threads) */
	apply_start = (thread_id - 1) * n / tg_size;
	apply_end   = thread_id * n / tg_size;

	/* Display the initial extended grid (thread 1 only) */
	if (thread_id == 1) {
		DPRINTF("\nExtended grid:\n");
		DPRINT_EXTENDED_GRID(extended_grid, n);
	}
	pthread_barrier_wait(barrier);

	/* ---- Main solving loop --------------------------------------- */
	do {
		local_is_changed = 0;

		for (depth = 1; depth <= max_depth; ++depth) {

			/* == Annotation phase (read-only on the grid) ========
			 * Thread 1 -> rows   (always)
			 * Thread 2 -> cols   (or thread 1 if tg_size < 2)
			 * Thread 3 -> boxes  (or thread 1 if tg_size < 3)
			 * Each uses its own already_propagated matrix.
			 * =================================================== */
			if (thread_id == 1)
				local_is_changed +=
					parallel_naked_candidates_rows(
						extended_grid, n,
						ap_rows[depth - 1], depth,
						0, n, deletions);

			if ((tg_size >= 2 && thread_id == 2) ||
			    (tg_size < 2 && thread_id == 1))
				local_is_changed +=
					parallel_naked_candidates_cols(
						extended_grid, n,
						ap_cols[depth - 1], depth,
						0, n, deletions);

			if ((tg_size >= 3 && thread_id == 3) ||
			    (tg_size < 3 && thread_id == 1))
				local_is_changed +=
					parallel_naked_candidates_boxes(
						extended_grid, n,
						ap_boxes[depth - 1], depth,
						0, n, deletions);

			/* Barrier: all annotations visible */
			pthread_barrier_wait(barrier);

			/* == Apply + zero phase ==============================
			 * Each thread bulk-deletes from its row slice and
			 * zeros the processed entries for the next depth. */
			if (apply_deletions(extended_grid, deletions, n,
					    apply_start, apply_end))
				local_is_changed = 1;

			/* Barrier: grid is consistent for the next depth */
			pthread_barrier_wait(barrier);
		}

		/* Reduce is_changed across all threads */
		thread_changed[thread_id - 1] = local_is_changed;
		pthread_barrier_wait(barrier);

		should_continue = 0;
		for (t = 0; t < tg_size; ++t)
			should_continue |= thread_changed[t];

	} while (should_continue);

	/* Progress report (thread 1 only) */
	if (thread_id == 1) {
		numbers_left = 0;
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++) {
				struct node *tmp = extended_grid[i][j];
				while (tmp) { numbers_left++; tmp = tmp->next; }
			}
		DPRINTF("Numbers left: %d\n", numbers_left);
		DPRINTF("Progress: %2.1f%%\n",
			(1.0 - (double)(numbers_left - n * n) /
			       (double)(n * n * n - n * n)) * 100);
	}

	free_propagation_matrix(ap_rows, n);
	free_propagation_matrix(ap_cols, n);
	free_propagation_matrix(ap_boxes, n);

	return 0;
}

/* ==================================================================
 *  Naked-candidates search functions
 *
 *  Identical to the serial versions except:
 *   - they accept start/end range parameters
 *   - they accept a deletions bitmask and call annotate_*()
 *     instead of propagate_*() so no linked-list is modified
 * ================================================================== */

int parallel_naked_candidates_rows(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row,
			  int *deletions)
{
	int i, j;
	int k;
	int l;
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (row) at depth %d\n", depth);

	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	changed = 0;

	for (i = start_row; i < end_row; ++i) {
		for (j = 0; j < n; ++j) {
			remaining_nodes = depth;
			candidates = NULL;
			l = 0;

			temp = extended_grid[i][j];
			DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
			DPRINT_LIST(temp);
			DPRINTF("\n");

			if (temp == NULL) {
				DPRINTF("\t\t - No values in this cell\n");
				continue;
			}

			if (size_list(temp) > depth) {
				DPRINTF("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			if (size_list(temp) == 1 && depth > 1)
				continue;

			DPRINTF("\t\tRight number of values\n");

			if (!already_propagated[i][j]) {
				temp2 = temp;
				do {
					candidates = append(candidates, temp2->data);
					if (candidates == NULL && temp2->data != 0) {
						fprintf(stderr, "Failed to append node in elimination\n");
						free(coord);
						return -1;
					}
					temp2 = temp2->next;
				} while (temp2 != NULL);

				coord[l].row = i;
				coord[l].column = j;
				l++;

				DPRINTF("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				DPRINTF("...%d\n", remaining_nodes);

				for (k = j + 1; k < n && remaining_nodes != 0; ++k) {
					if (size_list(extended_grid[i][k]) <= 1)
						continue;

					temp = extended_grid[i][k];
					n_difference = count_different_values(candidates, extended_grid[i][k]);

					DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", i + 1, k + 1, n_difference);

					if ((size_list(candidates) + n_difference) <= depth) {
						DPRINTF("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[i][k]);
						DPRINT_LIST(candidates);
						DPRINTF("\n\n");

						coord[l].row = i;
						coord[l].column = k;
						l++;

						--remaining_nodes;
						if (remaining_nodes == 0)
							break;
					}
				}

				if (remaining_nodes == 0) {
					DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l)
						DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					DPRINTF("\nValues to propagate: ");
					DPRINT_LIST(candidates);
					DPRINTF("\n");

					temp2 = candidates;
					while (temp2 != NULL) {
						annotate_row(deletions, n, coord,
							     depth, temp2->data);
						temp2 = temp2->next;
					}

					for (l = 0; l < depth; ++l)
						already_propagated[coord[l].row][coord[l].column] = 1;
					changed = 1;

					DPRINTF("\nAnnotation complete.\n\n");
				} else {
					DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				free_list(candidates);
			} else {
				DPRINTF("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates);
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}

int parallel_naked_candidates_cols(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_col, int end_col,
			  int *deletions)
{
	int i, j;
	int k;
	int l;
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	DPRINTF("\nElimination of naked candidates (column) at depth %d\n", depth);

	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	changed = 0;

	for (j = start_col; j < end_col; ++j) {
		for (i = 0; i < n; ++i) {
			remaining_nodes = depth;
			candidates = NULL;
			l = 0;

			temp = extended_grid[i][j];
			DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
			DPRINT_LIST(temp);
			DPRINTF("\n");

			if (temp == NULL) {
				DPRINTF("\t\t - No values in this cell\n");
				continue;
			}

			if (size_list(temp) > depth) {
				DPRINTF("\t\t - More than %d values in this cell\n",
				       depth);
				continue;
			}

			if (size_list(temp) == 1 && depth > 1)
				continue;

			DPRINTF("\t\tRight number of values\n");

			if (!already_propagated[i][j]) {
				temp2 = temp;
				do {
					candidates = append(candidates, temp2->data);
					if (candidates == NULL && temp2->data != 0) {
						fprintf(stderr, "Failed to append node in elimination\n");
						free(coord);
						return -1;
					}
					temp2 = temp2->next;
				} while (temp2 != NULL);

				coord[l].row = i;
				coord[l].column = j;
				l++;

				DPRINTF("Remaining nodes: %d", remaining_nodes);
				--remaining_nodes;
				DPRINTF("...%d\n", remaining_nodes);

				for (k = i + 1; k < n && remaining_nodes != 0; ++k) {
					if (size_list(extended_grid[k][j]) <= 1)
						continue;

					temp = extended_grid[k][j];
					n_difference = count_different_values(candidates, extended_grid[k][j]);

					DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, j + 1, n_difference);

					if ((size_list(candidates) + n_difference) <= depth) {
						DPRINTF("\t\t\tAdding new candidates to list...");
						candidates = add_new_candidates(candidates, extended_grid[k][j]);
						DPRINT_LIST(candidates);
						DPRINTF("\n\n");

						coord[l].row = k;
						coord[l].column = j;
						l++;

						--remaining_nodes;
						if (remaining_nodes == 0)
							break;
					}
				}

				if (remaining_nodes == 0) {
					DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
					for (l = 0; l < depth; ++l)
						DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
					DPRINTF("\nValues to propagate: ");
					DPRINT_LIST(candidates);
					DPRINTF("\n");

					temp2 = candidates;
					while (temp2 != NULL) {
						annotate_column(deletions, n, coord,
								depth, temp2->data);
						temp2 = temp2->next;
					}

					for (l = 0; l < depth; ++l)
						already_propagated[coord[l].row][coord[l].column] = 1;
					changed = 1;

					DPRINTF("\nAnnotation complete.\n\n");
				} else {
					DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
					       i + 1, j + 1);
				}

				free_list(candidates);
			} else {
				DPRINTF("\t - Cell [%d][%d] already propagated\n",
				       i + 1, j + 1);
				free_list(candidates);
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}

int parallel_naked_candidates_boxes(struct node ***extended_grid, int n,
			  int **already_propagated, int depth,
			  int start_row, int end_row,
			  int *deletions)
{
	int i, j;
	int k, m;
	int l;
	int remaining_nodes;
	int changed;
	int n_difference;
	struct node *candidates;
	struct node *temp;
	struct node *temp2;
	struct coordinates *coord;

	int sqrt_n;
	int box_row, box_col;
	int row_start, col_start;
	int given_row, given_col;

	DPRINTF("\nElimination of naked candidates (box) at depth %d\n", depth);

	coord = (struct coordinates *)malloc(depth *
					     sizeof(struct coordinates));
	if (coord == NULL) {
		fprintf(stderr, "Memory allocation failed\n");
		return -1;
	}

	sqrt_n = (int)sqrt(n);
	changed = 0;

	for (box_row = start_row / sqrt_n; box_row < end_row / sqrt_n; ++box_row) {
		for (box_col = 0; box_col < sqrt_n; ++box_col) {
			row_start = box_row * sqrt_n;
			col_start = box_col * sqrt_n;

			for (i = row_start; i < row_start + sqrt_n; ++i) {
				for (j = col_start; j < col_start + sqrt_n; ++j) {
					remaining_nodes = depth;
					candidates = NULL;
					l = 0;

					temp = extended_grid[i][j];
					DPRINTF("\tAt cell [%d][%d]: ", i + 1, j + 1);
					DPRINT_LIST(temp);
					DPRINTF("\n");

					if (temp == NULL) {
						DPRINTF("\t\t - No values in this cell\n");
						continue;
					}

					if (size_list(temp) > depth) {
						DPRINTF("\t\t - More than %d values in this cell\n",
							depth);
						continue;
					}

					if (size_list(temp) == 1 && depth > 1)
						continue;

					DPRINTF("\t\tRight number of values\n");

					if (!already_propagated[i][j]) {
						temp2 = temp;
						do {
							candidates = append(candidates, temp2->data);
							if (candidates == NULL && temp2->data != 0) {
								fprintf(stderr, "Failed to append node in elimination\n");
								free(coord);
								return -1;
							}
							temp2 = temp2->next;
						} while (temp2 != NULL);

						coord[l].row = i;
						coord[l].column = j;
						l++;

						DPRINTF("Remaining nodes: %d", remaining_nodes);
						--remaining_nodes;
						DPRINTF("...%d\n", remaining_nodes);

						given_row = i;
						given_col = j;
						for (k = row_start; k < row_start + sqrt_n && remaining_nodes != 0; ++k) {
							for (m = col_start; m < col_start + sqrt_n; ++m) {
								if (k > given_row || (k == given_row && m > given_col)) {
									if (size_list(extended_grid[k][m]) <= 1)
										continue;

									temp = extended_grid[k][m];
									n_difference = count_different_values(candidates, extended_grid[k][m]);

									DPRINTF("\t\t\tCell [%d][%d] - Difference: %d\n", k + 1, m + 1, n_difference);

									if ((size_list(candidates) + n_difference) <= depth) {
										DPRINTF("\t\t\tAdding new candidates to list...");
										candidates = add_new_candidates(candidates, extended_grid[k][m]);
										DPRINT_LIST(candidates);
										DPRINTF("\n\n");

										coord[l].row = k;
										coord[l].column = m;
										l++;

										--remaining_nodes;
										if (remaining_nodes == 0)
											break;
									}
								}
							}

							if (remaining_nodes == 0)
								break;
						}

						if (remaining_nodes == 0) {
							DPRINTF("\nFound naked tuple of size %d at cells: ", depth);
							for (l = 0; l < depth; ++l)
								DPRINTF("[%d][%d] ", coord[l].row + 1, coord[l].column + 1);
							DPRINTF("\nValues to propagate: ");
							DPRINT_LIST(candidates);
							DPRINTF("\n");

							temp2 = candidates;
							while (temp2 != NULL) {
								annotate_box(deletions, n, coord,
									     depth, temp2->data);
								temp2 = temp2->next;
							}

							for (l = 0; l < depth; ++l)
								already_propagated[coord[l].row][coord[l].column] = 1;
							changed = 1;

							DPRINTF("\nAnnotation complete.\n\n");
						} else {
							DPRINTF("\t\tDid not find enough matching cells for a tuple starting at [%d][%d]\n\n",
								i + 1, j + 1);
						}

						free_list(candidates);
					} else {
						DPRINTF("\t - Cell [%d][%d] already propagated\n",
							i + 1, j + 1);
						free_list(candidates);
					}
				}
			}
		}
	}

	free(coord);
	DPRINTF("\n");

	return changed;
}
