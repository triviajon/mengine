#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#include "src/kernel/expression.h"

// EMPTY_CONTEXT is a singleton, initialized with first call to
// context_create_empty()
static Context *EMPTY_CONTEXT = NULL;

// Returns a pointer to the empty context.
Context *context_create_empty();

bool context_contains_name(Context *context, char *name);

// Returns the outermost Expression in the context with the given name, or NULL
// if not found.
Expression *context_lookup_by_name(Context *context, char *name);

// Returns true if context is the empty context.
bool context_is_empty(Context *context);

// Add a variable-type binding to the context, and return the new context
Context *context_insert(Context *context, Expression *var_type);

// Adds n variable-type bindings to the context, in order of given arguments,
// and returns the new context.
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

// Applies the cut rule: given a context = gamma, a: A, delta, returns the context gamma, delta[x ->
// a], where delta[x -> a] is the context that results from substituting x with a in delta.
Context *context_cut(Context *context, Expression *x, Expression *a);

// Applies the context transformation from the substitution / cut rule.
// Given context = gamma+[x:A]+delta, returns gamma+delta[x->a] where x is
// eliminated and substituted through delta.
Context *context_replace(Context *context, Expression *x, Expression *a);

void context_free(Context *context);

// Returns true iff expr is well defined under the given context. This happpens
// when the context of expr is a subset of the given context.
bool valid_in_context(Expression *expr, Context *context);

// Returns true iff expr is a variable, and it is valid to add it to the given
// context. This happens when the context needed to define type(expr) is a
// subset of the given context.
bool valid_to_add_to_context(Expression *expr, Context *context);

#endif  // CONTEXT_H