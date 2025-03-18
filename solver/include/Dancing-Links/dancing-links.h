/* SPDX-License-Identifier: GPL-3.0 */

#ifndef DANCING_LINKS_H
#define DANCING_LINKS_H

/**
 * @struct node
 * @brief Structure for a node in the Dancing Links algorithm.
 *
 * This represents a node in Knuth's Dancing Links implementation of Algorithm X.
 * The nodes form a toroidal doubly-linked list structure.
 */
struct node {
	struct node *left; /* Pointer to the node on the left */
	struct node *right; /* Pointer to the node on the right */
	struct node *up; /* Pointer to the node above */
	struct node *down; /* Pointer to the node below */
	struct node *colHead; /* Pointer to the column header */
	int size; /* Size of the column (for column headers) */
	int id[3]; /* ID information (value, row, column) */
};

/**
 * @brief Chooses a column randomly from the header list.
 *
 * @param head Pointer to the head of the dancing links structure
 * @return Pointer to the randomly chosen column
 */
struct node *chooseRandomColumn(struct node *head);

/**
 * @brief Chooses a column deterministically (usually the one with minimum size).
 *
 * @param head Pointer to the head of the dancing links structure
 * @return Pointer to the chosen column
 */
struct node *chooseColumn(struct node *head);

/**
 * @brief Covers a column in the dancing links algorithm.
 *
 * @param col Pointer to the column to cover
 */
void cover(struct node *col);

/**
 * @brief Uncovers a column in the dancing links algorithm.
 *
 * @param col Pointer to the column to uncover
 */
void uncover(struct node *col);

#endif /* DANCING_LINKS_H */
