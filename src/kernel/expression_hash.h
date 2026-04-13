#ifndef EXPRESSION_HASH_H
#define EXPRESSION_HASH_H

#include "src/kernel/expression.h"

// Initialize the global intern table (idempotent).
void expression_intern_table_init(void);

// Look up an interned expression by structural inputs using a probe.
// The probe must have its tag and input fields set; derived fields (type, uplinks, etc.) are
// ignored. Returns the existing interned expression if found, NULL otherwise.
// Safe to call with a stack-allocated probe.
Expression *expression_intern_lookup(const Expression *probe);

// Insert a newly constructed expression into the intern table.
void expression_intern_insert(Expression *expr);

// Remove an expression from the intern table (called by free_expression).
// Only has effect for APP, LAMBDA, FORALL expressions.
void expression_intern_remove(const Expression *expr);

// Free the intern table at shutdown (call after expression_gc_shutdown).
void expression_intern_table_free(void);

#endif // EXPRESSION_HASH_H
