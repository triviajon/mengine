#include "expression.h"

#include "axiom.h"
#include "beta_reduction.h"
#include "subst.h"
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
Expression *constr_lambda_type(Expression *bound_variable, Expression *body) {
  Expression *type = init_forall_expression(bound_variable, get_expression_type(body));
  return type;
}

void hello() {
  return;
}

// Helper function to construct a app type
Expression *constr_app_type(Expression *func, Expression *arg) {
  Expression *func_type = get_expression_type(func); // something like Forall x: A, B
  Expression *variable = func_type->value.forall.bound_variable; // x
  Expression *expected_arg_type = get_expression_type(variable); // A
  Expression *actual_arg_type = get_expression_type(arg); // hopefully A, but we need to check.
  Expression *return_type = func_type->value.forall.body; // B

  if (congruence(actual_arg_type, expected_arg_type)) {
    return subst(return_type, variable, arg); // return B[x -> arg]
  }

  hello();
  congruence(actual_arg_type, expected_arg_type);
  
  return NULL; // Bad app constr, for now set type to NULL
}



Expression *init_var_expression(const char *name, Expression *type) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = VAR_EXPRESSION;
  expr->value.var.name = strdup(name);
  expr->value.var.type = type;
  expr->value.var.uplinks = dll_create();
  return expr;
}

Expression *init_lambda_expression(Expression *bound_variable, Expression *body) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = LAMBDA_EXPRESSION;
  expr->value.lambda.context = context_minus(get_expression_context(body), bound_variable);
  expr->value.lambda.bound_variable = bound_variable;
  expr->value.lambda.type = constr_lambda_type(bound_variable, body);
  expr->value.lambda.body = body;
  add_to_parents(body, new_uplink(expr, LAMBDA_BODY));
  expr->value.lambda.uplinks = dll_create();
  return expr;
}

Expression *init_app_expression(Expression *func, Expression *arg) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = APP_EXPRESSION;
  Context *combined_ctx = context_add(get_expression_context(func), get_expression_context(arg));
  expr->value.app.context = combined_ctx;
  expr->value.app.func = func;
  add_to_parents(func, new_uplink(expr, APP_FUNC));
  expr->value.app.arg = arg;
  add_to_parents(arg, new_uplink(expr, APP_ARG)); 
  expr->value.app.type = constr_app_type(func, arg);
  expr->value.app.cache = NULL;
  expr->value.app.uplinks = dll_create();
  return expr;
}

Expression *init_forall_expression(Expression *bound_variable, Expression *body) {
  Expression *expr = (Expression *)malloc(sizeof(Expression));
  expr->type = FORALL_EXPRESSION;
  expr->value.forall.context = context_minus(get_expression_context(body), bound_variable);
  expr->value.forall.bound_variable = bound_variable;
  expr->value.forall.type = init_prop_expression();
  expr->value.forall.body = body;
  add_to_parents(body, new_uplink(expr, FORALL_BODY));
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
  expr->value.hole.defining_context = context;
  expr->value.hole.return_type = type;
  expr->value.hole.uplinks = dll_create();
  return expr;
}

Expression *init_arrow_expression(Expression *lhs, Expression *rhs) {
  // lhs -> rhs <-> Forall _: lhs, rhs
  Expression *unnamed_variable = init_var_expression("_", lhs);
  return init_forall_expression(unnamed_variable, rhs);
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

Expression *get_expression_type(Expression *expression) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      return expression->value.var.type;
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
      return expression->value.hole.return_type;
  }
}

Context *get_expression_context(Expression *expression) {
  switch (expression->type) {
    case (VAR_EXPRESSION):
      return get_expression_context(expression->value.var.type);
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
      return expression->value.hole.defining_context;
  }
}

void fillHole(Expression *hole, Expression *term) {
  if (hole->type != HOLE_EXPRESSION) {
    return; 
  }

  DoublyLinkedList *holepars = hole->value.hole.uplinks; 
  for (int i = 0; i < dll_len(holepars); i++) {
    Uplink *uplink = dll_at(holepars, i)->data;
    switch (uplink->relation) {
      case (APP_FUNC):
        uplink->expression->value.app.func = term;
        break;
      case (APP_ARG):
        uplink->expression->value.app.arg = term;
        break;
      case (LAMBDA_BODY):
        uplink->expression->value.lambda.body = term;
        break;
      default:
        break;
    }
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
    free_expression(expr->value.hole.return_type);
    context_free(expr->value.hole.defining_context);
    dll_destroy(expr->value.forall.uplinks);
    free(expr);
  }
}