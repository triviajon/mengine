#ifndef CONTEXT_H
#define CONTEXT_H

#include <stdbool.h>
#include <stdlib.h>

#include "inductive.h"

// Forward declaration of Expression
typedef struct Expression Expression;

typedef struct Context {
  Expression *variable;  // binding variable
  Expression *type;      // and it's type
  struct Context
      *parent;  // if Γ[variable: type] is this context, then Γ is our parent.
} Context;

// Singleton
static Context *EMPTY_CONTEXT = NULL;

// Returns a pointer to the empty context, which is a persistent value.
Context *context_create_empty();

// Returns true if context is the empty context
bool context_is_empty(Context *context);

// Add a variable-type binding to the context, and return the new context
// containing a pointer to input context
Context *context_insert(Context *context, Expression *var, Expression *type);

Context *context_join(Context *contextA, Context *contextB);

// Add an inductive type to the context, and return the new context containing a
// pointer to input context
Context *context_insert_inductive(Context *context, Inductive *inductive);

// Look up the type of a variable in the context
Expression *context_lookup(Context *context, Expression *var);

// Returns true if context_A is an ancestor of context_B
bool context_is_ancestor(Context *context_A, Context *context_B);

// TODO: Unclear what we should be freeing.
void context_free(Context *context);

// Returns context->type
Expression *get_binding_variable_type(Context *context);

// Returns context->variable
Expression *get_binding_variable(Context *context);

// If the context has the form Γ[variable: type], it returns [variable: type].
// ? *context_head(Context *context);

// If the context has the form Γ[x: P], it returns a pointer to Γ.
Context *context_tail(Context *context);

#endif
