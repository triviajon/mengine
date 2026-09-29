#ifndef ROCQ_EXPORT_H
#define ROCQ_EXPORT_H

#include "src/kernel/expression.h"

// Render `target` and every entry of its context as a Rocq file wrapped in a Section.
// Context entries without a body, and proofs (bodies whose type is a Prop), become Axioms;
// other definitions keep their bodies. Subterms shared within a printed term that mention only
// context entries become let-bindings at the top of that term.
// Every variable and let-binding is named by its node address, not its user-facing name.
// Returns a heap-allocated string, or NULL (after printing an error) if `target` has no
// body, or a printed term contains a hole or a variable outside the context.
char *rocq_export(Expression *target);

#endif  // ROCQ_EXPORT_H
