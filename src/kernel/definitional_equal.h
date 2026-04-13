#ifndef DEFINITIONAL_EQUAL_H
#define DEFINITIONAL_EQUAL_H

#include "src/common/linear_map.h"
#include "src/kernel/expression.h"

bool definitional_equal(Expression *a, Expression *b);

/**
 * Returns true iff `expected` and `actual` are definitionally equal, treating
 * HOLE_EXPRESSIONs on the expected side as unification variables. Each hole is
 * recorded on first encounter and checked for consistency on subsequent
 * occurrences. Pure predicate — no holes are mutated.
 */
bool open_types_compatible(Expression *expected, Expression *actual);

/**
 * Like open_types_compatible, but also populates `holes` with every
 * hole→value assignment discovered during the traversal. `holes` must be
 * a caller-allocated, initially-empty LinearMap; entries are appended.
 * The caller can then apply these assignments via fill_hole to propagate
 * values to sibling goals (cascade fill).
 */
bool open_types_compatible_collecting(Expression *expected, Expression *actual,
                                      LinearMap *holes);

#endif  // DEFINITIONAL_EQUAL_H