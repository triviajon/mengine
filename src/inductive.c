#include "inductive.h"

Constructor *init_constructor(const char *name, Expression *type) {
    Constructor *constructor = (Constructor *)malloc(sizeof(Constructor));
    if (!constructor) return NULL;
    
    constructor->name = strdup(name);
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

Inductive *init_inductive(const char *name, DoublyLinkedList parameters, Expression *arity) {
    Inductive *inductive = (Inductive *)malloc(sizeof(Inductive));
    if (!inductive) return NULL;

    inductive->name = strdup(name);
    inductive->parameters = parameters;
    inductive->arity = arity;
    inductive->constructors = dll_create();

    return inductive;
}

void free_inductive(Inductive *inductive) {
    if (inductive) {
        free(inductive->name);

        dll_foreach(inductive->constructors, free_constructor);
        doubly_linked_list_free(inductive->constructors); 

        free_expression(inductive->arity);
        free(inductive);
    }
}