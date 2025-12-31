#ifndef BETA_REDUCTION_H
#define BETA_REDUCTION_H

#include "src/kernel/expression.h"

// Returns true if app_func and app_arg form a beta-reducible expression.
bool forms_beta_redex(Expression *app_func, Expression *app_arg);

// beta_reduce reduces a beta-redex, if (app_func app_arg) is a beta-redex.
// Returns the reduced expression, or NULL if the expression is not a beta-redex.
//
// If (app_func app_arg) := (fun (x: T) => t) u, then the typing rule is:
//
// ------------------------------------------------
// gamma |- (fun (x: T) => t) u is beta-reducible to t[x -> u].
Expression *beta_reduce(Context *gamma, Expression *app_func, Expression *app_arg);

// Normalizes an expression to a weak head normal form.
Expression *weak_head_normalize(Expression *expression);

#endif  // BETA_REDUCTION_H