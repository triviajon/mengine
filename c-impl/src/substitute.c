#include "substitute.h"

// In expr, replace instances of var by subst.
Expression *substitute(Expression *expr, char *var, Expression *subst) {
  switch (expr->type) {
  case (VAR_EXPRESSION):
    if (strcmp(expr->value.var.name, var) == 0) {
      return subst;
    } else {
      return expr;
    }
  case (LAMBDA_EXPRESSION):
    if (strcmp(expr->value.lambda.var->value.var.name, var) == 0) {
      return expr;
    } else {
      return init_lambda_expression(
          expr->value.lambda.var,
          substitute(expr->value.lambda.var_type, var, subst),
          substitute(expr->value.lambda.body, var, subst),
          expr->value.lambda.given_context);
    }
  case (APP_EXPRESSION):
    return init_app_expression(substitute(expr->value.app.func, var, subst),
                               substitute(expr->value.app.arg, var, subst),
                               expr->value.app.given_context);
  case (FORALL_EXPRESSION):
    return init_forall_expression(
        expr->value.forall.var, substitute(expr->value.forall.var_type, var, subst),
        substitute(expr->value.forall.arg, var, subst));
  case (TYPE_EXPRESSION):
    return init_type_expression();
  }
}
