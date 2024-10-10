#include "inductive.h"

Constructor *init_constructor(Expression *name, Expression *type) {
  Constructor *constructor = (Constructor *)malloc(sizeof(Constructor));
  if (!constructor) return NULL;

  constructor->name = name;
  constructor->type = type;

  return constructor;
}

void free_constructor(Constructor *constructor) {
  if (constructor) {
    free(constructor->name);
    free(constructor->type);
    free(constructor);
  }
}

Inductive *init_inductive(Expression *name, Context *parameters,
                          Expression *arity, DoublyLinkedList *constructors) {
  Inductive *inductive = (Inductive *)malloc(sizeof(Inductive));
  if (!inductive) return NULL;

  inductive->name = name;
  inductive->parameters = parameters;
  inductive->arity = arity;
  inductive->constructors = constructors;

  return inductive;
}

void free_inductive(Inductive *inductive) {
  if (inductive) {
    free(inductive->name);

    context_free(inductive->parameters);

    dll_foreach(inductive->constructors, free_constructor);
    dll_destroy(inductive->constructors);

    free_expression(inductive->arity);
    free(inductive);
  }
}