/* SPDX-License-Identifier: GPL-3.0 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../include/Dancing-Links/dancing-links.h"

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
