#ifndef TYPECHECK_H
#define TYPECHECK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expression.h"
#include "env.h"
#include "beta_reduction.h"

bool alpha_equivalent(Env *env, Expression *t1, Expression *t2);

Expression *substitute(Expression *expr, char *var, Expression *subst);

Step *substitute_rest_steps(Step *prog, char *var, Expression *subst);

Expression *typecheck_lambda(Expression *gamma, Expression *var, Expression *type, Expression *body);

Expression *typecheck_app(Expression *gamma, Expression *func, Expression *arg);

Expression *typecheck_forall(Expression *gamma, Expression *var, Expression *type, Expression *arg);

Expression *typecheck_expression_under_context(Expression *gamma, Expression *expr);

bool typecheck_context(Expression *gamma, Expression *delta);

Expression *typecheck_non_context(Expression *context, Expression *expr);

bool is_valid_context(Expression *expr);

Expression *typecheck_expression(Expression *expr);

Step *typecheck_let(char *name, Expression *expr, Step *next);

Step *typecheck_theorem(Theorem *theorem, Step *next);

void typecheck_prog(Step *step);

#endif // TYPECHECK_H
