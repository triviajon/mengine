#ifndef FIX_REDUCTION_H
#define FIX_REDUCTION_H

#include "src/kernel/expression.h"

// Returns true if expression is a fix-reducible expression.
// A fix-redex is a term of the form:
// (fix recursive_var (arg1: A1) (arg2: A2) ... (argn: An) {arg_decreasing} := body).
bool is_fix_reducible(Expression *expression);

// fix_reduce replaces a fix with a lambdas abstraction and replaces recursive calls to the symbol
// with the fix term.
//
// If expression is a fix-reducible expression, then the typing rule is:
//
// gamma |-
// ------------------------------------------------
// gamma |-
//   fix recursive_var (arg1: A1) (arg2: A2) ... (argn: An) {arg_decreasing} := body
//      is fix-reducible to
//   lambda (arg1: A1) (arg2: A2) ... (argn: An) {arg_decreasing} => body[recursive_var ->
//      fix recursive_var (arg1: A1) (arg2: A2) ... (argn: An) {arg_decreasing} := body]
//
// where gamma is the context which the fix expression is well-typed in.
Expression *fix_reduce(Expression *expression);

#endif  // FIX_REDUCTION_H