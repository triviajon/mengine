#ifndef BETA_REDUCTION_H
#define BETA_REDUCTION_H

#include "src/kernel/expression.h"

// Returns true if app_func and app_arg form a reducible expression.
bool forms_redex(Expression *app_func, Expression *app_arg);

// Reduces a reducible expression to a normal form.
Expression *reduce(Expression *app_func, Expression *app_arg);

// Normalizes an expression to a weak head normal form.
Expression *weak_head_normalize(Expression *expression);

#endif  // BETA_REDUCTION_H