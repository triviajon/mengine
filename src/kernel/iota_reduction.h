#ifndef IOTA_REDUCTION_H
#define IOTA_REDUCTION_H

#include "src/kernel/expression.h"

// Returns true if expression is a iota-reducible expression.
// An iota-redex is a term of the form:
// match (c' u1 ... un) with | c_1 => body_1 | ... | c_m => body_m end.
// where c' is a constructor of the inductive type I.
bool is_iota_reducible(Expression *expression);

// iota_reduce reduces a iota-reducible expression.
//
// If expression is a iota-reducible expression, then the typing rule is:
//
// gamma |-
// ------------------------------------------------
// gamma |-
//   match (c' u1 ... un) with | c_1 => body_1 | ... | c_m => body_m end.
//      is iota-reducible to
//   body_i[x_1 -> u_1, ..., x_n -> u_n]
//
Expression *iota_reduce(Context *gamma, Expression *expression);

#endif  // IOTA_REDUCTION_H