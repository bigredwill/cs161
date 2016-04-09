#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "linkedlist.h"

List *List_create()
{
	return (List *) calloc(1, sizeof(List));
}

void List_destroy(List *list)
{
	//macro defined in linkedlist.h

	//goes through and sets current to head in list.
	//We always free previous so that we don't lose our pointer to current!
	LIST_FOREACH(list, head, next, cur) {
		if(cur->prev) {
			free(cur->prev);
		}
	}
	free(list->tail);
	free(list);
}

void List_clear(List *list)
{
	LIST_FOREACH(list, head, next, cur) {
		free(cur->value);
	}
}

void List_clear_destroy(List *list)
{
	LIST_FOREACH(list, head, next, cur) {
		free(cur->value);
		if(cur->prev) {
			free(cur->prev);
		}
	}

	free(list->tail);
	free(list);
}

#define List_count(A) ((A)->count)
#define List_first(A) ((A)->head != NULL ? (A)->tail->value : NULL)
#define List_first(A) ((A)->head != NULL ? (A)->tail->value : NULL)

void List_push(List *list, void *value)
{
	ListNode *node = (ListNode *) calloc(1, sizeof(ListNode));
	node->value = value;

	if(list->tail == NULL) {
		list->head = node;
		list->tail = node;
	} else {
		list->tail->next = node;
		node->prev = list->tail;
		list->tail = node;
	}

	list->count++;
}

void *List_pop(List *list)
{
	ListNode *node = list->tail;
	if(node == NULL) {
		return NULL;
	} else {
		return List_remove(list, node);
	}
}

void List_unshift(List *list, void *value)
{
	ListNode *node = (ListNode *) calloc(1, sizeof(ListNode));
	node->value = value;

	if(list->head == NULL) {
		list->head = node;
		list->tail = node;
	} else {
		node->next = list->head;
		list->head->prev = node;
		list->head = node;
	}

	list->count++;

}
void *List_shift(List *list)
{

	ListNode *node = list->head;
	if(node == NULL) {
		return NULL;
	} else {
		return List_remove(list, node);
	}
}

void *List_remove(List *list, ListNode *node)
{
	void *result = NULL;
	if(!list->head && !list->tail) printf("List empty\n");
	if(!node) printf("List can't be NULL\n");

	if(node == list->head && node == list->tail) {
		list->head = NULL;
		list->tail = NULL;
	} else if(node == list->head) {
		list->head = node->next;
		if(list->head == NULL) printf("Invalid list head.\n");
		list->head->prev = NULL;
	} else if(node == list->tail) {
		list->tail = node->prev;
		if(list->tail == NULL) printf("Invalid list tail.\n");
		list->tail->next = NULL;
	} else {
		ListNode *next_node = node->next;
		ListNode *prev_node = node->prev;
		next_node->prev = prev_node;
		prev_node->next = next_node;
	}

	list->count--;
	result = node->value;
	free(node);

	return result;
}
