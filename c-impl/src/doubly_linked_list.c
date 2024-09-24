#include "doubly_linked_list.h"

DoublyLinkedList *dll_create(void)
{
    DoublyLinkedList *list;
    if (!(list = malloc(sizeof(DoublyLinkedList))))
        return NULL;

    list->head = NULL;
    list->tail = NULL;
    return list;
}

void dll_destroy(DoublyLinkedList *list)
{
    DLLNode *current = list->head;
    DLLNode *next;

    while (current)
    {
        next = current->next;
        free(current);
        current = next;
    }

    free(list);
}

DLLNode *dll_insert_at_tail(DoublyLinkedList *list, DLLNode *node)
{
    if (!node)
        return NULL;

    if (list->tail)
    {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    else
    {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    }

    return node;
}

DLLNode *dll_remove_tail(DoublyLinkedList *list)
{
    if (!list->tail)
        return NULL;

    DLLNode *node = list->tail;

    if (node->prev)
    {
        list->tail = node->prev;
        list->tail->next = NULL;
    }
    else
    {
        list->head = list->tail = NULL;
    }

    free(node);
    return node;
}

DLLNode *dll_remove_head(DoublyLinkedList *list)
{
    if (!list->head)
        return NULL;

    DLLNode *node = list->head;

    if (node->next)
    {
        list->head = node->next;
        list->head->prev = NULL;
    }
    else
    {
        list->head = list->tail = NULL;
    }

    free(node);
    return node;
}

DLLNode *dll_insert_at_head(DoublyLinkedList *list, DLLNode *node)
{
    if (!node)
        return NULL;

    if (list->head)
    {
        node->next = list->head;
        node->prev = NULL;
        list->head->prev = node;
        list->head = node;
    }
    else
    {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    }

    return node;
}

DLLNode *dll_search(DoublyLinkedList *list, void *data)
{
    DLLNode *node = list->head;

    while (node)
    {
        if (data == node->data)
        {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

int dll_len(DoublyLinkedList *list) {
    DLLNode *node = list->head;
    int count = 0;

    while (node) {
        count++;
    }

    return count;
}



DLLNode *dll_at(DoublyLinkedList *list, int index)
{
    DLLNode *node = list->head;

    if (index < 0)
    {
        node = list->tail;
        index = -index - 1;
        while (index-- && node)
        {
            node = node->prev;
        }
    }
    else
    {
        while (index-- && node)
        {
            node = node->next;
        }
    }

    return node;
}

void dll_print(DoublyLinkedList *list)
{
    DLLNode *node = list->head;

    while (node)
    {
        printf("%p\n", node->data);
        node = node->next;
    }
}

void dll_foreach(DoublyLinkedList *list, void (*func)(void *data))
{
    DLLNode *node = list->head;

    while (node)
    {
        func(node->data);
        node = node->next;
    }
}

DLLNode *dll_new_node(void *val)
{
    DLLNode *self;
    if (!(self = malloc(sizeof(DLLNode))))
        return NULL;
    self->prev = NULL;
    self->next = NULL;
    self->data = val;
    return self;
}