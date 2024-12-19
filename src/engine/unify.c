#include "unify.h"

Expression *get_type_eq(Expression *eq_type) {
  if (eq_type == NULL) {
    return NULL;
  }
  if (eq_type->type != APP_EXPRESSION) {
    return NULL;
  }
  if (eq_type->value.app.func->type != APP_EXPRESSION) {
    return NULL;
  }
  Expression *expected_eq = eq_type->value.app.func->value.app.func->value.app.func;
  if (expected_eq != eq) {
    return NULL;
  }

  Context *eq_type_ctx = get_expression_context(eq_type);
  Expression *eq_type_expr = eq_type->value.app.func->value.app.func->value.app.arg;
  return get_expression_type(eq_type_ctx, eq_type_expr);
}



Expression *get_lhs_eq(Expression *eq_type) {
  if (eq_type == NULL) {
    return NULL;
  }
  if (eq_type->type != APP_EXPRESSION) {
    return NULL;
  }
  if (eq_type->value.app.func->type != APP_EXPRESSION) {
    return NULL;
  }
  Expression *expected_eq = eq_type->value.app.func->value.app.func->value.app.func;
  if (expected_eq != eq) {
    return NULL;
  }
  return eq_type->value.app.func->value.app.arg;
}

Expression *get_rhs_eq(Expression *eq_type) {
  if (eq_type == NULL) {
    return NULL;
  }
  if (eq_type->type != APP_EXPRESSION) {
    return NULL;
  }
  if (eq_type->value.app.func->type != APP_EXPRESSION) {
    return NULL;
  }
  Expression *expected_eq = eq_type->value.app.func->value.app.func->value.app.func;
  if (expected_eq != eq) {
    return NULL;
  }
  return eq_type->value.app.arg;
}

Expression *instantiate_lemma_type(Context *context, Expression *lemma_ty) {
  switch (lemma_ty->type) {
    case (FORALL_EXPRESSION): {
      Expression *bound_var =
          get_binding_variable(lemma_ty->value.forall.bound_variable);
      Expression *bound_var_ty =
          get_binding_variable_type(lemma_ty->value.forall.bound_variable);
      Expression *hole = init_hole_expression(bound_var->value.var.name,
                                              bound_var_ty, context);
      Expression *new_body = new_reduce(lemma_ty, hole);
      // The result of inner_inst should be the body of lemma with binding
      // variables substituted for holes
      Expression *inner_inst = instantiate_lemma_type(context, new_body);
      return inner_inst;
    }
    default:
      return lemma_ty;
  }
}

Expression *instantiate_lemma(Context *context, Expression *lemma) {
  Expression *lemma_ty = context_lookup(context, lemma);
  return instantiate_lemma_type(context, lemma_ty);
}

bool contains_holes(Expression *expr) {
  switch (expr->type) {
    case (LAMBDA_EXPRESSION):
      return contains_holes(expr->value.lambda.bound_variable->type) ||
             contains_holes(expr->value.lambda.body);
    case (FORALL_EXPRESSION):
      return contains_holes(expr->value.forall.bound_variable->type) ||
             contains_holes(expr->value.forall.body);
    case (APP_EXPRESSION):
      return contains_holes(expr->value.app.func) ||
             contains_holes(expr->value.app.arg);
    case (HOLE_EXPRESSION):
      return true;
    default:
      return false;
  }
}

DoublyLinkedList *_list_holes(Expression *expr, DoublyLinkedList *curr) {
  switch (expr->type) {
    case (LAMBDA_EXPRESSION): {
      curr = _list_holes(expr->value.lambda.bound_variable->type, curr); 
      curr = _list_holes(expr->value.lambda.body, curr);
      return curr;
    }
    case (FORALL_EXPRESSION): {
      curr = _list_holes(expr->value.forall.bound_variable->type, curr); 
      curr = _list_holes(expr->value.forall.body, curr);
      return curr;
    }
    case (APP_EXPRESSION): {
      curr = _list_holes(expr->value.app.func, curr); 
      curr = _list_holes(expr->value.app.arg, curr);
      return curr;
    }
    case (HOLE_EXPRESSION): {
      if (dll_search(curr, expr) == NULL) {
        dll_insert_at_tail(curr, dll_new_node(expr));
        return curr;
      } else {
        return curr;
      }
    }
    default:
      return curr;
  }
}

DoublyLinkedList *list_holes(Expression *expr) {
  DoublyLinkedList *lst = dll_create();
  return _list_holes(expr, lst);
}

Expression *_unify(Context *exprA_ctx, Expression *exprA, Context *exprB_ctx, Expression *exprB, Context *hole_to_fill) {
  switch (exprA->type) {
    case VAR_EXPRESSION: {
      if (exprA != hole_to_fill->variable) {
        // Don't care...
        return NULL;
      }

      Expression *exprB_ty = get_expression_type(exprB_ctx, exprB);
      Expression *expected_ty = hole_to_fill->type;
      if (equivalent_under_computation(exprB_ty, expected_ty)) {
        return exprB;
      }
      return NULL;
    }
    case APP_EXPRESSION: {
      if (exprB->type != APP_EXPRESSION) {
        return NULL;
      }
      
      Expression *new_func = _unify(exprA_ctx, exprA->value.app.func, exprB_ctx, exprB->value.app.func, hole_to_fill);
      if (new_func != NULL) {
        return new_func;
      }

      Expression *new_arg = _unify(exprA_ctx, exprA->value.app.arg, exprB_ctx, exprB->value.app.arg, hole_to_fill);
      if (new_arg != NULL) {
        return new_arg;
      }

      return NULL;
    }
    default:
      return NULL;
  }
}

Map *unify(Context *exprA_ctx, Expression *exprA, Expression *exprB) {
  DoublyLinkedList *binding_vars = dll_create(); // A list of var: type pairs (contexts)
  Expression *current = exprA;
  while (current->type == FORALL_EXPRESSION) {
    dll_insert_at_tail(binding_vars, dll_new_node(current->value.forall.bound_variable));
    current = current->value.forall.body;
  }
  
  Map *result = map_new();
  Expression *body = get_lhs_eq(current);
  DLLNode *current_binding_var_node = binding_vars->head;
  Context *exprB_ctx = get_expression_context(exprB);
  while (current_binding_var_node != NULL) {
    Context *current_binding_var = current_binding_var_node->data;
    // Greedily search for a subexpression in exprB with the correct type
    Expression *binded = _unify(exprA_ctx, body, exprB_ctx, exprB, current_binding_var);
    if (binded == NULL) {
      map_free(result);
      return NULL;
    }
    map_set(result, current_binding_var->variable, binded);
    current_binding_var_node = current_binding_var_node->next;
  }
  return result;
}

Expression *instantiate_lemma_with_bindings(Context *ctx, Expression *lemma, Expression *lemma_ty, Map *binders) {
  Expression *final_expr = lemma;
  Expression *curr_forall = lemma_ty;
  while (curr_forall->type == FORALL_EXPRESSION) {
    Expression *binding_var = curr_forall->value.forall.bound_variable->variable;
    Expression *binding_result = map_get(binders, binding_var);
    final_expr = init_app_expression(ctx, final_expr, binding_result);
    curr_forall = new_reduce(curr_forall, binding_result);
  }
  return final_expr;
}