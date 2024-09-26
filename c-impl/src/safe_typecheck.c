#include "safe_typecheck.h"

Expression *get_expr_type(Expression *gamma, Expression *expr) {
  switch (expr->type) {
  case (VAR_EXPRESSION):
    return lookup_in_context(gamma, expr);
  case (LAMBDA_EXPRESSION):
    return expr->value.lambda.type;
  case (APP_EXPRESSION):
    return expr->value.app.type;
  case (FORALL_EXPRESSION):
    return init_type_expression(); // TODO
  case (TYPE_EXPRESSION):
    return expr;
  }
}

Expression *safe_synthesize_lambda_type(Expression *gamma, Expression *var,
                                        Expression *var_type,
                                        Expression *body) {
  Expression *new_gamma = set_in_context(gamma, var, var_type);
  Expression *body_type = get_expr_type(new_gamma, body);
  return init_forall_expression(var, var_type, body_type);
}

Expression *safe_synthesize_app_type(Expression *gamma, Expression *func,
                                     Expression *arg) {
  Expression *func_type = get_expr_type(gamma, func);
  Expression *arg_type = get_expr_type(gamma, func);

  if (func_type->type == FORALL_EXPRESSION &&
      alpha_equivalent(arg_type, func_type->value.forall.var_type)) {
    return substitute(func_type->value.forall.arg,
                      func_type->value.forall.var->value.var.name, arg);
  } else {
    // panic!
    return NULL;
  }
}
