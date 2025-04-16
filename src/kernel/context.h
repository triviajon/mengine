#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#include "doubly_linked_list.h"

// Forward declaration of Expression
typedef struct Expression Expression;

// A (var) expression can be re-used in multiple Context chains, as long as
// the chain remains valid.
typedef struct Context
{
  Expression *var_type;
  struct Context *parent; // if Γ[variable: type] is this context, then Γ is our parent.
  int length;
} Context;

// Singleton, initialized with first call to context_create_empty()
static Context *EMPTY_CONTEXT = NULL;

// Returns a pointer to the empty context.
Context *context_create_empty();

bool context_contains_name(Context *context, char *name);

// Returns true if context is the empty context.
bool context_is_empty(Context *context);

// Add a variable-type binding to the context, and return the new context
Context *context_insert(Context *context, Expression *var_type);

// Adds n variable-type bindings to the context, in order of given arguments, and returns the new context.
Context *context_insert_n(Context *context, int n, ...);

// Gets the size of the content. Empty context has size 0.
int context_size(Context *context);

// Returns true if contextA is an ancestor of contextB. I.e., returns
// true iff contextA ==z contextB or contextA == contextB->...->parent.
bool context_is_ancestor(Context *contextA, Context *contextB);

// Finds the parent context whose context_node contains var, or NULL in case of
// failure.
Context *context_find(Context *context, Expression *var);

// Creates a list of context_A ancestors starting at the empty context ...
// context_A
DoublyLinkedList *context_ancestors(Context *context_A);

// Finds least common ancestor of context_A and context_B
Context *context_LCA(Context *context_A, Context *context_B);

// Given two contexts, returns the "sum" of the contexts.
// Specifically each variable binding in context_B, starting from the empty context, 
// will be inserted to the end of context_A if it is not already found in context_A.
Context *context_add(Context *context_A, Context *context_B);

// If subtrahend is not a variable found in the given context, this function returns context unchanged.
// Otherwise, this function removes the subtrahend node (and its dependencies) from a context tree. 
// It ensures that only the minimal set of nodes is removed, preserving the context tree's integrity. 
Context *context_minus(Context *context, Expression *subtrahend);

void context_free(Context *context);

// Returns true iff expr is well defined under the given context. This happpens 
// when the context of expr is a subset of the given context.
bool valid_in_context(Expression *expr, Context *context);

// Returns true iff expr is a variable, and it is valid to add it to the given context.
// This happens when the context needed to define type(expr) is a subset of the given context.
bool valid_to_add_to_context(Expression *expr, Context *context);

#endif // CONTEXT_H