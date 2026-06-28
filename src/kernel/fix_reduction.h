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

// fix_reduce_app reduces an application `(((fix ...) a0) a1) ... an` whose head is a
// fix, but only when the guard condition holds: the decreasing argument (after being
// reduced to weak head normal form via `whnf`) is in constructor head normal form.
// This mirrors Coq's iota/fix rule and prevents non-termination when a fixpoint is
// applied to a symbolic (variable) recursive argument.
//
// Returns the unfolded-and-reapplied term on success, or NULL when `app` is not a
// fix application, the decreasing argument has not been supplied, or it is not
// constructor-headed (i.e. the redex is stuck). `whnf` may be NULL to skip reducing
// the decreasing argument (useful when the caller has already normalized it).
Expression *fix_reduce_app(Expression *app, Expression *(*whnf)(Expression *));

#endif  // FIX_REDUCTION_H
