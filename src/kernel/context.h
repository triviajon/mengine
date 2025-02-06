#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>
#include <stdlib.h>

#include "doubly_linked_list.h"

// Forward declaration of Expression
typedef struct Expression Expression;

// A (var) expression can be re-used in multiple Context chains, as long as
// the chain remains valid.
typedef struct Context {
  Expression *var_type;
  struct Context *parent;  // if Γ[variable: type] is this context, then Γ is our parent.
} Context;

// Singleton, initialized with first call to context_create_empty()
static Context *EMPTY_CONTEXT = NULL;

// Returns a pointer to the empty context.
Context *context_create_empty();

// Returns true if context is the empty context.
bool context_is_empty(Context *context);

// Add a variable-type binding to the context, and return the new context
Context *context_insert(Context *context, Expression *var_type);

// Gets the size of the content. Empty context has size 0.
int context_size(Context *context);

// Returns true if contextA is an ancestor of contextB. I.e., returns
// true iff contextA == contextB or contextA == contextB->...->parent.
bool context_is_ancestor(Context *contextA, Context *contextB);

// Finds the parent context whose context_node contains var, or NULL in case of
// failure.
Context *context_find(Context *context, Expression *var);

// Creates a list of context_A ancestors starting at the empty context ...
// context_A
DoublyLinkedList *context_ancestors(Context *context_A);

// Finds least common ancestor of context_A and context_B
Context *context_LCA(Context *context_A, Context *context_B);

Context *context_add(Context *context_A, Context *context_B);

// Suppose context has the form [variable1: type1] ... [subtrahend: subtrahendtype]...  
// Then this function returns the context up until, but not including, the subtrahend,
// or the original context if the subtrahend is not found.
Context *context_minus(Context *context, Expression *subtrahend);

void context_free(Context *context);

bool valid_in_context(Expression *expr, Context *context);

#endif  // CONTEXT_H