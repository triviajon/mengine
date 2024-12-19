#include "beta_reduction.h"
#include "context.h"

Expression *reduce_body(Expression *body, Expression *old, Expression *old_ty, Expression *new) {
  switch (body->type) {
    case (FORALL_EXPRESSION): {
      // Forall x: A, B ->  Forall x: A[old -> new], B[old -> new]
      Expression *forall_body = body->value.forall.body;
      Context *result_context = context_combine(body->value.forall.bound_variable, old, old_ty, new);
      return init_forall_expression(result_context, reduce_body(forall_body, old, old_ty, new));
    }
    case (LAMBDA_EXPRESSION): {
      // lambda x: A, B ->  lambda x: A[old -> new], B[old -> new]
      Expression *lambda_body = body->value.lambda.body;
      Context *result_context = context_combine(body->value.lambda.bound_variable, old, old_ty, new);
      return init_lambda_expression(result_context, reduce_body(lambda_body, old, old_ty, new));
    }
    case (APP_EXPRESSION): {
      Context *result_context = context_combine(get_expression_context(body), old, old_ty, new);
      return init_app_expression(result_context, reduce_body(body->value.app.func, old, old_ty, new), reduce_body(body->value.app.arg, old, old_ty, new));
    }
    case (VAR_EXPRESSION): {
      return (body == old) ? new : body;
    }
    case (TYPE_EXPRESSION): 
    case (PROP_EXPRESSION): {
      return body;
    }
    default:
      return NULL;
  }
}

Expression *reduce(Expression *app_func, Expression *app_arg) {
  if (app_func->type != LAMBDA_EXPRESSION) {
    // printf("Expression is not a redex.");
    return NULL;
  }

  Expression *body = app_func->value.lambda.body;
  Expression *old = get_binding_variable(app_func->value.lambda.bound_variable);
  Expression *old_ty = get_binding_variable_type(app_func->value.lambda.bound_variable);
  Expression *new = app_arg;
  return reduce_body(body, old, old_ty, new);
}