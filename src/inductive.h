#ifndef INDUCTIVE_H
#define INDUCTIVE_H

#include "doubly_linked_list.h"
#include "expression.h"

typedef struct {
    char *name;
    Expression *type;
} Constructor;

typedef struct {
    char *name;
    DoublyLinkedList parameters; // DLL should hold pointers to Expressions
    Expression *arity;
    DoublyLinkedList *constructors; // DLL should hold pointers to Constructors
} Inductive;

Constructor *init_constructor(const char *name, Expression *type);
void free_constructor(Constructor *constructor);

Inductive *init_inductive(const char *name, DoublyLinkedList parameters, Expression *arity);
void free_inductive(Inductive *inductive);


#endif // INDUCTIVE_H
