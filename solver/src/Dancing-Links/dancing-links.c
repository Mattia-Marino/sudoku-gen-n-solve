/* SPDX-License-Identifier: GPL-3.0 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/Dancing-Links/dancing-links.h"

/* Choose a random column from among several good candidates */
struct node *chooseRandomColumn(struct node *head)
{
	struct node *candidates[5];
	struct node *j;
	int sizes[5];
	int count;
	int i;
	int max_idx;
	int random_idx;
	int initialized;

	count = 0;

	/* Initialize with large values */
	for (i = 0; i < 5; i++) {
		candidates[i] = NULL;
		sizes[i] = INT_MAX;
	}

	/* Find the 5 columns with smallest sizes */
	for (j = head->right; j != head; j = j->right) {
		/* Skip if column size is 0 */
		if (j->size <= 0)
			continue;

		/* Find the largest size in our array */
		max_idx = 0;

		for (i = 1; i < 5; i++) {
			if (sizes[i] > sizes[max_idx])
				max_idx = i;
		}

		/* Replace if current column has smaller size */
		if (j->size < sizes[max_idx] || count < 5) {
			if (count < 5)
				count++;
			candidates[max_idx] = j;
			sizes[max_idx] = j->size;
		}
	}

	/* Choose random column from candidates */
	if (count > 0) {
		/* Make sure random number generator is initialized */
		initialized = 0;

		if (!initialized) {
			srand(time(NULL));
			initialized = 1;
		}

		random_idx = rand() % count;
		return candidates[random_idx];
	}

	/* Fallback to first column */
	return head->right;
}

struct node *chooseColumn(struct node *head)
{
	struct node *column;
	struct node *j;

	column = head->right;

	for (j = column->right; j != head; j = j->right) {
		if (j->size < column->size)
			column = j;
	}
	return column;
}

void cover(struct node *column)
{
	struct node *i;
	struct node *j;

	column->left->right = column->right;
	column->right->left = column->left;

	for (i = column->down; i != column; i = i->down) {
		for (j = i->right; j != i; j = j->right) {
			j->down->up = j->up;
			j->up->down = j->down;
			(j->colHead->size)--;
		}
	}
}

void uncover(struct node *column)
{
	struct node *i;
	struct node *j;

	for (i = column->up; i != column; i = i->up) {
		for (j = i->left; j != i; j = j->left) {
			(j->colHead->size)++;
			j->down->up = j;
			j->up->down = j;
		}
	}

	column->left->right = column;
	column->right->left = column;
}
