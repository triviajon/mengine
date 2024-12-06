#ifndef BETA_REDUCTION_H
#define BETA_REDUCTION_H

#include "expression.h"

Expression *reduce_body(Expression *body, Expression *old, Expression *old_ty, Expression *new);
Expression *reduce(Expression *app_func, Expression *app_arg);

#endif  // BETA_REDUCTION_H