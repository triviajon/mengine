#include "beta_reduction.h"

// Forward declaration
int upcopy(Expression *new_child, Expression *parent, Relation relation);

static void var_clean(Expression *var_expression) {
  if (var_expression->type == VAR_EXPRESSION) {
    free(var_expression->value.var.name);
    dll_destroy(var_expression->value.var.uplinks);
  }
  free(var_expression);
}

int free_dead_expression(Expression *expression) {
  if (expression == NULL)
    return 0;

  if (expression->type == LAMBDA_EXPRESSION) {
    var_clean(expression->value.lambda.var);
    free_dead_expression(expression->value.lambda.body);
  } else if (expression->type == APP_EXPRESSION) {
    free_dead_expression(expression->value.app.func);
    free_dead_expression(expression->value.app.arg);
  } else if (expression->type == VAR_EXPRESSION) {
    var_clean(expression);
  } else {
    printf("Unknown expression type %d\n", expression->type);
    return 1;
  }
  return 0;
}

int replace_child(DoublyLinkedList *old_parents, Expression *new_child) {
  DLLNode *current = old_parents->head;
  while (current != NULL) {
    Uplink *uplink = (Uplink *)current->data;
    Expression *parent = uplink->expression;
    Relation relation = uplink->relation;

    switch (relation) {
    case LAMBDA_BODY:
      if (parent->type == LAMBDA_EXPRESSION) {
        parent->value.lambda.body = new_child;
      } else {
        printf("Invalid relation for expression type: %d\n", parent->type);
        return 1;
      }
      break;
    case APP_FUNC:
      if (parent->type == APP_EXPRESSION) {
        parent->value.app.func = new_child;
      } else {
        printf("Invalid relation for expression type: %d\n", parent->type);
        return 1;
      }
      break;
    case APP_ARG:
      if (parent->type == APP_EXPRESSION) {
        parent->value.app.arg = new_child;
      } else {
        printf("Invalid relation for expression type: %d\n", parent->type);
        return 1;
      }
      break;
    default:
      printf("Relation between child/parent is invalid, got %d\n", relation);
      return 1;
    }

    // Update the new child's uplinks
    Uplink *new_uplink = malloc(sizeof(Uplink));
    if (new_uplink == NULL) {
      printf("Memory allocation failed.\n");
      return 1;
    }
    new_uplink->expression = parent;
    new_uplink->relation = relation;
    dll_insert_at_tail(new_child->value.app.uplinks, dll_new_node(new_uplink));

    current = current->next;
  }

  // When we're done, we are good to clear the old_parents
  DLLNode *node_to_delete;
  while (old_parents->head != NULL) {
    node_to_delete = old_parents->head;
    old_parents->head = old_parents->head->next;
    free(node_to_delete->data);
    free(node_to_delete);
  }
  old_parents->tail = NULL;
  return 0;
}

Expression *scandown(Expression *expression, Expression *argterm,
                     DoublyLinkedList *varpars, Expression **topapp) {
  if (expression->type == LAMBDA_EXPRESSION) {
    Expression *body_prime =
        scandown(expression->value.lambda.body, argterm, varpars, topapp);
    Expression *l_prime =
        init_lambda_expression(expression->value.lambda.var,
                               expression->value.lambda.type, body_prime);
    return l_prime;
  } else if (expression->type == VAR_EXPRESSION) {
    topapp = NULL;
    return argterm;
  } else {
    // Must be APP_EXPRESSION
    Expression *a_prime = init_app_expression(expression->value.app.func,
                                              expression->value.app.arg);
    expression->value.app.cache = a_prime;
    DLLNode *current = varpars->head;
    while (current != NULL) {
      Uplink *uplink = (Uplink *)current->data;
      upcopy(argterm, uplink->expression, uplink->relation);
    }
    *topapp = expression;
    return a_prime;
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
Expression *beta_reduction(Expression *context, Expression *expression) {
   if (expression->type != APP_EXPRESSION) {
    // printf("Expression must be an app expression.");
    return expression;
  } 

  Expression *app_func = expression->value.app.func;

  if (app_func->type != LAMBDA_EXPRESSION) {
    // printf("Expression is not a redex.");
    return expression;
  }

  Expression *ans;

  if (dll_len(app_func->value.lambda.uplinks) == 1) {
    Expression *lambda_var = app_func->value.lambda.var;
    replace_child(lambda_var->value.var.uplinks, expression->value.app.arg);
    ans = app_func;
  } else if (dll_len(app_func->value.lambda.var->value.var.uplinks) == 0) {
    ans = app_func->value.lambda.body;
  } else {
    Expression *top_app = NULL;
    Expression *lambda_var = app_func->value.lambda.var;
    ans = scandown(expression, expression->value.app.arg,
                   lambda_var->value.var.uplinks, &top_app);
    if (top_app != NULL) {
      clear_caches(app_func, top_app);
    }
  }

  replace_child(expression->value.app.uplinks, ans);
  free_dead_expression(expression);
  return ans;
}
#pragma clang diagnostic pop

int upcopy(Expression *new_child, Expression *parent, Relation relation) {
  if (relation == LAMBDA_BODY) {
    Expression *new_lambda = init_lambda_expression(
        parent->value.lambda.var, parent->value.lambda.type, new_child);
    DLLNode *current = parent->value.lambda.uplinks->head;
    while (current != NULL) {
      Uplink *uplink = (Uplink *)current->data;
      upcopy(new_lambda, uplink->expression, uplink->relation);
      current = current->next;
    }
    return 0;
  } else if (relation == APP_FUNC) {
    if (parent->value.app.cache == NULL) {
      Expression *new_app =
          init_app_expression(new_child, parent->value.app.arg);
      parent->value.app.cache = new_app;
      DLLNode *current = parent->value.app.uplinks->head;
      while (current != NULL) {
        Uplink *uplink = (Uplink *)current->data;
        upcopy(new_app, uplink->expression, uplink->relation);
        current = current->next;
      }
    } else {
      Expression *app_cache = parent->value.app.cache;
      app_cache->value.app.func = new_child;
    }
    return 0;
  } else if (relation == APP_ARG) {
    if (parent->value.app.cache == NULL) {
      Expression *new_app =
          init_app_expression(parent->value.app.func, new_child);
      parent->value.app.cache = new_app;
      DLLNode *current = parent->value.app.uplinks->head;
      while (current != NULL) {
        Uplink *uplink = (Uplink *)current->data;
        upcopy(new_app, uplink->expression, uplink->relation);
        current = current->next;
      }
    } else {
      Expression *app_cache = parent->value.app.cache;
      app_cache->value.app.arg = new_child;
    }
    return 0;
  } else {
    printf("Relation between child/parent is invalid, got %d\n", relation);
    return 1;
  }
}

int clear_caches(Expression *reduced_lambda_expression,
                 Expression *top_app_expression) {
  if (reduced_lambda_expression->type != LAMBDA_EXPRESSION ||
      top_app_expression->type != APP_EXPRESSION) {
    printf(
        "reduced_lambda_expression must be of type LAMBDA_EXPRESSION (got %d)\n"
        "top_app_expression must be of type APP_EXPRESSION (got %d)\n",
        reduced_lambda_expression->type, top_app_expression->type);
  }

  // For Lambda body and App arg
  clean_up(reduced_lambda_expression, LAMBDA_BODY);
  clean_up(top_app_expression, APP_ARG);
  return 0;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
void clean_up(Expression *expression, Relation relation) {
  if (expression->type == LAMBDA_EXPRESSION) {
    // Clean lambda body
    Expression *body = expression->value.lambda.body;
    if (body->type == APP_EXPRESSION) {
      clean_up(body, APP_FUNC);
    }
    free(expression->value.lambda.body);
  } else if (expression->type == APP_EXPRESSION) {
    // Clean app func and arg
    Expression *func = expression->value.app.func;
    Expression *arg = expression->value.app.arg;
    if (func->type == LAMBDA_EXPRESSION) {
      clean_up(func, LAMBDA_BODY);
    }
    if (arg->type == LAMBDA_EXPRESSION) {
      clean_up(arg, LAMBDA_BODY);
    }
    free(expression->value.app.func);
    free(expression->value.app.arg);
  }
}
#pragma clang diagnostic push
