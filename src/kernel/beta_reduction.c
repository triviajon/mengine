#include "beta_reduction.h"
#include "context.h"

Expression *reduce_body(Expression *body, Expression *old, Expression *new) {
  switch (body->type) {
    case (FORALL_EXPRESSION): {
      // Forall x: A, B ->  Forall x: A[old -> new], B[old -> new]
      Expression *forall_body = body->value.forall.body;
      Context *result_context = context_combine(body, old, new);
      Expression *bv = get_binding_variable(body->value.forall.bound_variable);
      Expression *bt = get_binding_variable_type(body->value.forall.bound_variable);
      Context *with_binding = context_insert(result_context, bv, reduce_body(bt, old, new));
      return init_forall_expression(with_binding, reduce_body(forall_body, old, new));
    }
    case (APP_EXPRESSION): {
      Context *result_context = context_combine(body, old, new);
      return init_app_expression(result_context, reduce_body(body->value.app.func, old, new), reduce_body(body->value.app.arg, old, new));
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
  Expression *new = app_arg;
  return reduce_body(body, old, new);
}