#ifndef BETA_REDUCTION_H
#define BETA_REDUCTION_H

#include "expression.h"
#include "subst.h"

bool forms_redex(Expression *app_func, Expression *app_arg);

Expression *reduce(Expression *app_func, Expression *app_arg);

#endif  // BETA_REDUCTION_H