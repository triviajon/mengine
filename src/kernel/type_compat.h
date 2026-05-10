#ifndef TYPE_COMPAT_H
#define TYPE_COMPAT_H

#include "src/common/linear_map.h"
#include "src/kernel/expression.h"

/**
 * Returns true iff `expected` and `actual` are compatible in `context`, treating
 * HOLE_EXPRESSIONs on the expected side as unification variables. Each hole is
 * recorded on first encounter and checked for consistency on later occurrences.
 * Pure predicate - no holes are mutated.
 */
bool open_types_compatible_in_context(Context *context, Expression *expected, Expression *actual);

/**
 * Like open_types_compatible_in_context, but also populates `holes` with every
 * hole->value assignment discovered during the traversal. `holes` must be a
 * caller-allocated, initially-empty LinearMap; entries are appended. The caller
 * can then apply these assignments via fill_hole to propagate values to sibling
 * goals.
 */
bool open_types_compatible_collecting_in_context(Context *context, Expression *expected,
                                                 Expression *actual, LinearMap *holes);

#endif  // TYPE_COMPAT_H
