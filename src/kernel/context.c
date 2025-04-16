#include "context.h"

#include "expression.h"

Context *context_create_empty() {
  if (EMPTY_CONTEXT == NULL) {
    EMPTY_CONTEXT = (Context *)malloc(sizeof(Context));
    EMPTY_CONTEXT->var_type = NULL;
    EMPTY_CONTEXT->parent = NULL;
    EMPTY_CONTEXT->length = 0;
  }
  return EMPTY_CONTEXT;
}

bool context_is_empty(Context *context) { return context == EMPTY_CONTEXT; }

bool context_contains_name(Context *context, char *name) {
  Context *curr = context;
  while (!context_is_empty(curr)) {
    if (strcmp(curr->var_type->value.var.name, name) == 0) {
      return true;
    }
    curr = curr->parent;
  }
  return false;
}

Context *context_insert(Context *context, Expression *var_type) {
  if (var_type->type != VAR_EXPRESSION) {
    return NULL;
  }

  if (context_find(context, var_type) != NULL) {
    return context;
  }

  Expression *expr_type = var_type->value.var.type;
  if (!valid_in_context(expr_type, context)) {
    return NULL;
  }

  Context *new_ctx = (Context *)malloc(sizeof(Context));
  new_ctx->var_type = var_type;
  new_ctx->parent = context;
  new_ctx->length = context->length + 1;
  add_to_parents(var_type, new_uplink2(new_ctx, CTX_VAR));
  return new_ctx;
}

Context *context_insert_n(Context *context, int n, ...) {
  va_list argptr;
  va_start(argptr, n);

  Context *final = context;
  for (int i = 0; i < n; i++) {
    Expression *curr = va_arg(argptr, Expression*);
    final = context_insert(final, curr);
    if (final == NULL) {
      return NULL;
    }
  }

  return final;
}

int context_size(Context *context) {
  return context->length;
}

bool context_is_ancestor(Context *contextA, Context *contextB) {
  if (contextA == contextB) {
    return true;
  } else if (context_is_empty(contextA)) {
    return true;
  }

  Context *curr_ctxB = contextB;
  while (!context_is_empty(curr_ctxB)) {
    if (contextA == curr_ctxB) {
      return true;
    }
    curr_ctxB = curr_ctxB->parent;
  }

  return false;
}

Context *context_find(Context *context, Expression *var) {
  Context *curr = context;
  while (!context_is_empty(curr)) {
    if (curr->var_type == var) {
      return curr;
    }

    curr = curr->parent;
  }
  return NULL;
}

DoublyLinkedList *context_ancestors(Context *context_A) {
  DoublyLinkedList *list = dll_create();
  Context *curr_context = context_A;
  while (!context_is_empty(curr_context)) {
    Expression *node = curr_context->var_type;
    dll_insert_at_head(list, dll_new_node(node));
    curr_context = curr_context->parent;
  }
  return list;
}

DoublyLinkedList *context_ancestors_until(Context *context_A, Expression *until) {
  DoublyLinkedList *list = dll_create();
  Context *curr_context = context_A;
  while (!context_is_empty(curr_context) && curr_context->var_type != until) {
    Expression *node = curr_context->var_type;
    dll_insert_at_head(list, dll_new_node(node));
    curr_context = curr_context->parent;
  }
  return list;
}

Context *context_LCA(Context *context_A, Context *context_B);

void context_free(Context *context) { 
  free(context);
}

Context *context_add(Context *context_A, Context *context_B) {
  if (context_A == NULL || context_B == NULL) {
    return NULL;
  }

  if (context_A == context_B) {
    return context_A;
  }

  Context *start_context = context_A;
  Context *add_from_context = context_B;
  // Context *start_context = (context_A->length < context_B->length) ? context_B : context_A;
  // Context *add_from_context = (context_A->length < context_B->length) ? context_A : context_B;

  DoublyLinkedList *add_from_ancestors = context_ancestors(add_from_context);
  int n = dll_len(add_from_ancestors);

  Context *result = start_context;
  for (int i = 0; i < n; i++) {
    Expression *curr_add_from_expr = dll_at(add_from_ancestors, i)->data;
    if (context_find(start_context, curr_add_from_expr) == NULL) {
      result = context_insert(result, curr_add_from_expr);
    }
  }

  return result;
}

Context *context_minus(Context *context, Expression *subtrahend) {
  Context *until_subtra = context_find(context, subtrahend);
  if (until_subtra == NULL) {
    return context;
  } 

  // We know for sure that the given context contains subtrahend. 
  DoublyLinkedList *given_ancestors = context_ancestors_until(context, subtrahend);
  int n = dll_len(given_ancestors);
  Context *result = until_subtra->parent;

  for (int i = 0; i < n; i++) {
    Expression *curr_vartype = dll_at(given_ancestors, i)->data;
    if (valid_to_add_to_context(curr_vartype, result)) {
      result = context_insert(result, curr_vartype);
    }
  }
  return result;
}

bool valid_in_context(Expression *expr, Context *context) {
  Context *curr_expr_ctx = get_expression_context(expr);
  if (context_is_ancestor(curr_expr_ctx, context)) {
    return true;
  }

  while (!context_is_empty(curr_expr_ctx)) {
    Expression *curr_var = curr_expr_ctx->var_type;
    if (context_find(context, curr_var) == NULL) {
      return false;
    }
    curr_expr_ctx = curr_expr_ctx->parent;
  }
  return true;
}

bool valid_to_add_to_context(Expression *expr, Context *context) {
  if (expr->type != VAR_EXPRESSION) {
    return false;
  }

  Expression *expr_type = get_expression_type(expr);
  return valid_in_context(expr_type, context);
}

