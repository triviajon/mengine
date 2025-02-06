#include "beta_reduction.h"
#include "context.h"

bool forms_redex(Expression *app_func, Expression *app_arg) {
  return app_func != NULL && (app_func->type == LAMBDA_EXPRESSION) && app_arg != NULL;
}

bool is_redex(Expression *app_expr) {
  if (app_expr->type != APP_EXPRESSION) {
    return false;
  }

  Expression *app_func = app_expr->value.app.func;
  return (app_func->type == LAMBDA_EXPRESSION);
}

Expression *reduce(Expression *app_func, Expression *app_arg) {
  if (app_func->type != LAMBDA_EXPRESSION) {
    // printf("Expression is not a redex.");
    return NULL;
  }

  Expression *body = app_func->value.lambda.body;
  Expression *old = app_func->value.lambda.bound_variable;
  Expression *new = app_arg;
  return subst(body, old, new);
}