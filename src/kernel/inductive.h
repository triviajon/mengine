#ifndef INDUCTIVE_H
#define INDUCTIVE_H

#include "doubly_linked_list.h"

// Forward declaration of Expression
typedef struct Expression Expression;
void free_expression(Expression *expr);

// Forward declaration of Context
typedef struct Context Context;
void context_free(Context *context);

typedef struct {
  Expression *name; // Should be a variable expression
  Expression *type;
} Constructor;

typedef struct {
  Expression *name; // Should be a variable expression
  Context *parameters;  // Mapping of variables to types
  Expression *arity;
  DoublyLinkedList *constructors;  // DLL should hold pointers to Constructors
} Inductive;

Constructor *init_constructor(Expression *name, Expression *type);
void free_constructor(Constructor *constructor);

Inductive *init_inductive(Expression *name, Context *parameters,
                          Expression *arity, DoublyLinkedList *constructors);
void free_inductive(Inductive *inductive);

#endif  // INDUCTIVE_H
