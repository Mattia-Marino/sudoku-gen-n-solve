/* SPDX-License-Identifier: GPL-3.0 */

#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/**
 * Structure representing a node in a linked list.
 */
struct node {
	int data;		/* Value stored in the node */
	struct node *next;	/* Pointer to the next node */
};

int size_list(struct node *head);

int count_different_values(struct node *a, struct node *b);

/**
 * Checks if a node is the last one in the list.
 *
 * @param node Pointer to the node to check
 * @return 1 if the node is the last one in the list, 0 otherwise
 */
int is_last_node(struct node *node);

/**
 * Creates a new node with the specified data.
 *
 * @param data The integer value to store in the node
 * @return Pointer to the newly created node, or NULL if allocation fails
 */
struct node *create_node(int data);

/**
 * Appends a new node with the given data to the end of the list.
 *
 * @param head Pointer to the head of the list
 * @param data The integer value to store in the new node
 * @return Pointer to the head of the updated list
 */
struct node *append(struct node *head, int data);

/**
 * Removes the first node containing the specified value.
 *
 * @param head Pointer to the head of the list
 * @param value The value to search for and remove
 * @return Pointer to the head of the updated list
 */
struct node *delete_at_given_value(struct node *head, int value);

struct node *delete_all_but_head(struct node *head);

struct node *add_new_candidates(struct node *candidates, struct node *new);

/**
 * Prints all elements of the linked list.
 *
 * @param head Pointer to the head of the list
 */
void print_list(struct node *head);

/*
int get_head_value(struct node *head);
*/

/**
 * Frees the memory allocated for the linked list.
 *
 * @param head Pointer to the head of the list
 */
void free_list(struct node *head);

#endif /* LINKED_LIST_H */