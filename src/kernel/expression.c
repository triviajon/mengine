#include "expression.h"

#include "axiom.h"
#include "beta_reduction.h"
#include "context.h"

void add_to_parents(Expression *expression, Uplink *uplink) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      dll_insert_at_head(expression->value.var.uplinks, dll_new_node(uplink));
      break;
    case (LAMBDA_EXPRESSION):
      dll_insert_at_head(expression->value.lambda.uplinks,
                         dll_new_node(uplink));
      break;
    case (APP_EXPRESSION):
      dll_insert_at_head(expression->value.app.uplinks, dll_new_node(uplink));
      break;
    case (FORALL_EXPRESSION):
      dll_insert_at_head(expression->value.forall.uplinks,
                         dll_new_node(uplink));
      break;
    case (TYPE_EXPRESSION):
    case (PROP_EXPRESSION):
      break;
    case (HOLE_EXPRESSION):
      dll_insert_at_head(expression->value.hole.uplinks, dll_new_node(uplink));
      break;
  }
}

Uplink *new_uplink(Expression *parent, Relation relation) {
  Uplink *new_uplink = malloc(sizeof(Uplink));
  if (new_uplink == NULL) {
    return NULL;
  }
  new_uplink->expression = parent;
  new_uplink->relation = relation;
  return new_uplink;
}

// Helper function to construct a lambda type
Expression *constr_lambda_type(Context *context, Expression *body) {
  Expression *type =
      init_forall_expression(context, get_expression_type(context, body));

  // Need to make sure that the body's context is an ancestor of the lambda's
  // context
  if (context_is_ancestor(get_expression_context(body), context)) {
    return type;
  }
  return NULL;  // Bad lambda constr, for now set type to NULL
}

// Helper function to construct a app type
Expression *constr_app_type(Context *context, Expression *func,
                            Expression *arg) {
  Expression *func_type =
      get_expression_type(context, func);  // something like Forall x: A, B
  Expression *variable =
      get_binding_variable(func_type->value.forall.bound_variable);  // x
  Expression *expected_arg_type =
      get_binding_variable_type(func_type->value.forall.bound_variable);  // A
  Expression *actual_arg_type =
      get_expression_type(context, arg);  // hopefully A, but we need to check.

  if (equivalent_under_computation(actual_arg_type, expected_arg_type)) {
    return new_reduce(func_type, arg);
  }
  return NULL;  // Bad app constr, for now set type to NULL
}

Expression *new_reduce(Expression *func_type, Expression *arg) {
  Context *ft_var_ctx = func_type->value.forall.bound_variable;
  Expression *ft_body = func_type->value.forall.body;

  // If func_type is Forall x: A, B, then lambda_over_var is lambda x: A, B.
  // This allows us to pair it with an argument and then beta-reduce.
  Expression *lambda_over_var = init_lambda_expression(ft_var_ctx, ft_body);

  return reduce(lambda_over_var, arg);
}

Expression *init_var_expression(const char *name) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = VAR_EXPRESSION;
  expr->value.var.name = strdup(name);
  expr->value.var.uplinks = dll_create();
  expr->value.var.context = NULL;
  return expr;
}

Expression *init_lambda_expression(Context *context, Expression *body) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = LAMBDA_EXPRESSION;
  expr->value.lambda.context = context->parent;
  expr->value.lambda.bound_variable = context;
  expr->value.lambda.type = constr_lambda_type(context, body);
  expr->value.lambda.body = body;
  expr->value.lambda.uplinks = dll_create();
  return expr;
}

Expression *init_app_expression(Context *context, Expression *func,
                                Expression *arg) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = APP_EXPRESSION;
  expr->value.app.context = context;
  expr->value.app.func = func;
  add_to_parents(func, new_uplink(expr, APP_FUNC));
  expr->value.app.arg = arg;
  add_to_parents(arg, new_uplink(expr, APP_ARG));
  expr->value.app.type = constr_app_type(context, func, arg);
  expr->value.app.cache = NULL;
  expr->value.app.uplinks = dll_create();
  return expr;
}

Expression *init_forall_expression(Context *context, Expression *body) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = FORALL_EXPRESSION;
  expr->value.forall.context = context->parent;
  expr->value.forall.bound_variable = context;
  expr->value.forall.type = init_prop_expression();
  expr->value.forall.body = body;
  expr->value.forall.uplinks = dll_create();
  return expr;
}

Expression *init_prop_expression() {
  if (PROP == NULL) {
    PROP = (Expression *)malloc(sizeof(Expression));
    PROP->type = PROP_EXPRESSION;
    PROP->value.type.uplinks = dll_create();
  }
  return PROP;
}

Expression *init_type_expression() {
  if (TYPE == NULL) {
    TYPE = (Expression *)malloc(sizeof(Expression));
    TYPE->type = TYPE_EXPRESSION;
    TYPE->value.type.uplinks = dll_create();
  }
  return TYPE;
}

Expression *init_hole_expression(char *name, Expression *type,
                                 Context *context) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = HOLE_EXPRESSION;
  expr->value.hole.name = name;
  expr->value.hole.type = type;
  expr->value.hole.context = context;
  expr->value.forall.uplinks = dll_create();
  return expr;
}

Expression *init_arrow_expression(Context *context, Expression *lhs,
                                  Expression *rhs) {
  Context *unnamed_ctx = context_insert(context, init_var_expression("_"), lhs);
  return init_forall_expression(unnamed_ctx, rhs);
}

DoublyLinkedList *get_expression_uplinks(Expression *expression) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      return expression->value.var.uplinks;
    case (LAMBDA_EXPRESSION):
      return expression->value.lambda.uplinks;
    case (APP_EXPRESSION):
      return expression->value.app.uplinks;
    case (FORALL_EXPRESSION):
      return expression->value.forall.uplinks;
    case (TYPE_EXPRESSION):
      return expression->value.type.uplinks;
    case (PROP_EXPRESSION):
      return expression->value.prop.uplinks;
    case (HOLE_EXPRESSION):
      return expression->value.hole.uplinks;
  }
}

Expression *get_expression_type(Context *context, Expression *expression) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      return context_lookup(context, expression);
    case (LAMBDA_EXPRESSION):
      return expression->value.lambda.type;
    case (APP_EXPRESSION):
      return expression->value.app.type;
    case (FORALL_EXPRESSION):
      return expression->value.forall.type;
    case (TYPE_EXPRESSION):
      return expression;
    case (PROP_EXPRESSION):
      return expression;
    case (HOLE_EXPRESSION):
      return expression->value.hole.type;
  }
}

Context *get_expression_context(Expression *expression) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      return expression->value.var.context;
    case (LAMBDA_EXPRESSION):
      return expression->value.lambda.context;
    case (APP_EXPRESSION):
      return expression->value.app.context;
    case (FORALL_EXPRESSION):
      return expression->value.forall.context;
    case (TYPE_EXPRESSION):
      return context_create_empty();
    case (PROP_EXPRESSION):
      return context_create_empty();
    case (HOLE_EXPRESSION):
      return expression->value.hole.context;
  }
}

// Forward declarations. No need to expose them in expression.h.
void free_var_expression(Expression *expr);
void free_lambda_expression(Expression *expr);
void free_app_expression(Expression *expr);
void free_forall_expression(Expression *expr);
void free_type_expression(Expression *expr);
void free_prop_expression(Expression *expr);
void free_hole_expression(Expression *expr);

void free_expression(Expression *expr) {
  switch (expr->type) {
    case (VAR_EXPRESSION):
      free_var_expression(expr);
      break;
    case (LAMBDA_EXPRESSION):
      free_lambda_expression(expr);
      break;
    case (APP_EXPRESSION):
      free_app_expression(expr);
      break;
    case (FORALL_EXPRESSION):
      free_forall_expression(expr);
      break;
    case (TYPE_EXPRESSION):
      free_type_expression(expr);
      break;
    case (PROP_EXPRESSION):
      free_prop_expression(expr);
      break;
    case (HOLE_EXPRESSION):
      free_hole_expression(expr);
      break;
  }
}

void free_var_expression(Expression *expr) {
  if (expr && expr->type == VAR_EXPRESSION) {
    free(expr->value.var.name);
    dll_destroy(expr->value.var.uplinks);
    free(expr);
  }
}

void free_lambda_expression(Expression *expr) {
  if (expr && expr->type == LAMBDA_EXPRESSION) {
    context_free(expr->value.lambda.context);
    free_expression(expr->value.lambda.type);
    free_expression(expr->value.lambda.body);
    dll_destroy(expr->value.lambda.uplinks);
    free(expr);
  }
}

void free_app_expression(Expression *expr) {
  if (expr && expr->type == APP_EXPRESSION) {
    free_expression(expr->value.app.func);
    free_expression(expr->value.app.arg);
    if (expr->value.app.cache) {
      free_expression(expr->value.app.cache);
    }
    free_expression(expr->value.app.type);
    context_free(expr->value.app.context);
    dll_destroy(expr->value.app.uplinks);
    free(expr);
  }
}

void free_forall_expression(Expression *expr) {
  if (expr && expr->type == FORALL_EXPRESSION) {
    context_free(expr->value.forall.context);
    free_expression(expr->value.forall.type);
    free_expression(expr->value.forall.body);
    dll_destroy(expr->value.forall.uplinks);
    free(expr);
  }
}

void free_type_expression(Expression *expr) {
  if (expr && expr->type == TYPE_EXPRESSION) {
    dll_destroy(expr->value.type.uplinks);
    free(expr);
  }
}

void free_prop_expression(Expression *expr) {
  if (expr && expr->type == PROP_EXPRESSION) {
    dll_destroy(expr->value.type.uplinks);
    free(expr);
  }
}

void free_hole_expression(Expression *expr) {
  if (expr && expr->type == HOLE_EXPRESSION) {
    free(expr->value.hole.name);
    free_expression(expr->value.hole.type);
    context_free(expr->value.hole.context);
    dll_destroy(expr->value.forall.uplinks);
    free(expr);
  }
}