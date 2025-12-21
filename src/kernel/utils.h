#ifndef UTILS_H
#define UTILS_H

#include "src/kernel/expression.h"

typedef struct Context Context;

// Stringify an expression to a Coq-ready string.
char *stringify_expression(Expression *expression);

// Stringify an expression to a Coq-ready string using let-bindings to represent
// the implicit sharing in the DAG representation of the expression.
char *stringify_expression_with_let(Expression *expression);

// Stringify a context to a Coq-ready string.
char *stringify_context(Context *context);

// Stringify a context until a given context, effectively visualizing the
// difference between the two contexts. This requires that until be an ancestor
// of context. Returns NULL if until is not an ancestor of context.
char *stringify_context_until(Context *context, Context *until);

// Shorthand for stringify_expression.
char *se(Expression *expression);

// Shorthand for stringify_context.
char *sc(Context *context);

#endif  // UTILS_H