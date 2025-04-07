#include <stdio.h>
#include <stdlib.h>

#include "../include/linked_list.h"

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

int is_last_node(struct node *node)
{
	return (node->next == NULL);
}

void print_list(struct node *node)
{
	while (node != NULL) {
		printf("%d -> ", node->data);
		node = node->next;
	}
}

int get_head_value(struct node *head)
{
	if (head != NULL)
		return head->data;
	else
		return -1;
}

void free_list(struct node *head)
{
	struct node *temp;

	while (head != NULL) {
		temp = head;
		head = head->next;
		free(temp);
	}
}

/*
int main(int argc, char **argv)
{
	struct node *head;

	head = NULL;
	head = append(head, 1);
	head = append(head, 2);
	head = append(head, 3);

	printf("List: ");
	print_list(head);
	printf("NULL\n");

	head = delete_at_given_value(head, 1);

	printf("After deletion: ");
	print_list(head);
	printf("NULL\n");

	free_list(head);

	return 0;
}
	*/
