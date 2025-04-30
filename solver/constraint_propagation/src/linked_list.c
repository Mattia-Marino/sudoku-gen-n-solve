#include <stdio.h>
#include <stdlib.h>

#include "../include/linked_list.h"

int size_list(struct node *head)
{
	int count;

	count = 0;
	while (head != NULL) {
		count++;
		head = head->next;
	}

	return count;
}

int count_different_values(struct node *a, struct node *b)
{
	int count;
	int found;
	struct node *temp_a;
	struct node *temp_b;

	/* Count elements in b that are not in a */
	temp_b = b;
	count = 0;
	while (temp_b != NULL) {
		found = 0;
		temp_a = a;

		while (temp_a != NULL) {
			if (temp_b->data == temp_a->data) {
				found = 1;
				break;
			}
			temp_a = temp_a->next;
		}

		if (!found) {
			printf("The element %d of b was not found in a\n", temp_b->data);
			++count;
		}
		
		temp_b = temp_b->next;
	}

	/* Return the number of elements in b not in a */
	return count;
}

int is_last_node(struct node *node)
{
	return (node->next == NULL);
}

struct node *create_node(int data)
{
	struct node *new_node = (struct node *)malloc(sizeof(struct node));
	if (new_node == NULL)
		return NULL;

	new_node->data = data;
	new_node->next = NULL;
	return new_node;
}

struct node *append(struct node *head, int data)
{
	struct node *new_node = create_node(data);
	if (new_node == NULL)
		return NULL;

	if (head == NULL)
		return new_node;

	struct node *last = head;

	while (last->next != NULL) {
		last = last->next;
	}

	last->next = new_node;

	return head;
}

struct node *delete_at_given_value(struct node *head, int value)
{
	struct node *temp = head, *prev = NULL;;

	if (temp != NULL && temp->data == value) {
		head = temp->next;
		free(temp);
		return head;
	}

	while (temp != NULL && temp->data != value) {
		prev = temp;
		temp = temp->next;
	}

	if (temp == NULL)
		return head;

	prev->next = temp->next;
	free(temp);

	return head;
}

struct node *delete_all_but_head(struct node *head)
{
	struct node *temp = head;
	struct node *to_delete;

	if (head == NULL)
		return NULL;

	while (temp->next != NULL) {
		to_delete = temp->next;
		temp->next = to_delete->next;
		free(to_delete);
	}

	return head;
}

struct node *add_new_candidates(struct node *candidates, struct node *new)
{
	int found;
	struct node *temp_old;
	struct node *temp_new;

	temp_new = new;
	while (temp_new != NULL) {
		found = 0;
		temp_old = candidates;

		while (temp_old != NULL) {
			if (temp_old->data == temp_new->data) {
				found = 1;
				break;
			}

			temp_old = temp_old->next;
		}

		if (!found)
			candidates = append(candidates, temp_new->data);
		
		temp_new = temp_new->next;
	}

	return candidates;
}

void print_list(struct node *node)
{
	while (node != NULL) {
		printf("%d -> ", node->data);
		node = node->next;
	}
}

/*
int get_head_value(struct node *head)
{
	if (head != NULL)
		return head->data;
	else
		return -1;
}
		*/

void free_list(struct node *head)
{
	struct node *temp;

	while (head != NULL) {
		temp = head;
		head = head->next;
		free(temp);
	}
}
