#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>

// Credit: https://github.com/clibs/list/tree/master

typedef struct DLLNode {
    void *data;
    struct DLLNode *next;
    struct DLLNode *prev;
} DLLNode;

typedef struct DoublyLinkedList {
    DLLNode *head;
    DLLNode *tail;
} DoublyLinkedList;

/*
 * Allocate a new DoublyLinkedList. NULL on failure.
 */
DoublyLinkedList *dll_create(void);

/*
 * Free the list.
 * @list: Pointer to the list 
 */
void dll_destroy(DoublyLinkedList *list);

/*
 * Return the number of nodes in the list.
 * @list: Pointer to the list
 * @return: Number of nodes
 */
int dll_len(DoublyLinkedList *list);

/*
 * Append the given node to the list
 * and return the node, NULL on failure.
 * @list: Pointer to the list for appending node
 * @node: the node to append
 */
DLLNode *dll_insert_at_tail(DoublyLinkedList *list, DLLNode *node);

/*
 * Return / detach the last node in the list, or NULL.
 * @list: Pointer to the list for removing node
 */
DLLNode *dll_remove_tail(DoublyLinkedList *list);

/*
 * Return / detach the first node in the list, or NULL.
 * @list: Pointer to the list for removing node
 */
DLLNode *dll_remove_head(DoublyLinkedList *list);

/*
 * Prepend the given node to the list
 * and return the node, NULL on failure.
 * @list: Pointer to the list for prepending node
 * @node: the node to prepend
 */
DLLNode *dll_insert_at_head(DoublyLinkedList *list, DLLNode *node);

/*
 * Return the node associated with data or NULL.
 * @list: Pointer to the list for finding given data
 * @data: Data to find 
 */
DLLNode *dll_search(DoublyLinkedList *list, void *data);

/*
 * Return the node at the given index or NULL.
 * @list: Pointer to the list for finding given index 
 * @index: the index of node in the list
 */
DLLNode *dll_at(DoublyLinkedList *list, int index);

/*
 * Print all nodes in the list.
 * @list: Pointer to the list for printing 
 */
void dll_print(DoublyLinkedList *list);

/*
 * Apply function to each node's data in the list.
 * @list: Pointer to the list for applying function 
 * @func: Function to apply to each node's data
 */
void dll_foreach(DoublyLinkedList *list, void (*func)(void *data));

/*
 * Allocates a new list_node_t. NULL on failure.
 */

DLLNode *dll_new_node(void *val);

#endif // DOUBLY_LINKED_LIST_H
