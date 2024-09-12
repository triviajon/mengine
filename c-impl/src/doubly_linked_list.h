#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

typedef struct DLLNode {
  void *data;
  struct DLLNode *next;
  struct DLLNode *prev;
} DLLNode;

typedef struct DoublyLinkedList {
  DLLNode *head;
  DLLNode *tail;
} DoublyLinkedList;

DoublyLinkedList *dll_create();
void dll_destroy(DoublyLinkedList *list);
int dll_len(DoublyLinkedList *list);
void dll_insert_at_head(DoublyLinkedList *list, void *data);
void dll_insert_at_tail(DoublyLinkedList *list, void *data);
void dll_delete_node(DoublyLinkedList *list, DLLNode *node);
DLLNode *dll_search(DoublyLinkedList *list, void *data);
void dll_print(DoublyLinkedList *list);
void dll_foreach(DoublyLinkedList *list, void (*func)(void *data));

#endif // DOUBLY_LINKED_LIST_H
