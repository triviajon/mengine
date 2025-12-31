#ifndef DELTA_REDUCTION_H
#define DELTA_REDUCTION_H

#include "src/kernel/expression.h"

// Returns true if expression is a delta-reducible expression (i.e., non-opaque).
bool is_delta_reducible(Expression *expression);

// delta_reduce reduces a delta-reducible expression.
//
// If expression is a variable defined in context gamma, then the typing rule is:
//
// gamma |-
// [x := t : T] is in gamma
// ------------------------------------------------
// gamma |- x ->delta t
Expression *delta_reduce(Expression *expression);

#endif  // DELTA_REDUCTION_H