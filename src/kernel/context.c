#include "context.h"

#include "expression.h"

Context *context_create_empty() {
  if (EMPTY_CONTEXT == NULL) {
    EMPTY_CONTEXT = (Context *)malloc(sizeof(Context));
    EMPTY_CONTEXT->var_type = NULL;
    EMPTY_CONTEXT->parent = NULL;
  }
  return EMPTY_CONTEXT;
}

bool context_is_empty(Context *context) { return context == EMPTY_CONTEXT; }

Context *context_insert(Context *context, Expression *var_type) {
  if (var_type->type != VAR_EXPRESSION) {
    return NULL;
  }

  Expression *expr_type = var_type->value.var.type;
  if (!valid_in_context(expr_type, context)) {
    return NULL;
  }

  Context *new_ctx = (Context *)malloc(sizeof(Context));
  new_ctx->var_type = var_type;
  new_ctx->parent = context;
  return new_ctx;
}

int context_size(Context *context) {
  int count = 0;
  Context *curr = context;
  while (!context_is_empty(curr)) {
    curr = curr->parent;
    count += 1;
  }
  return count;
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
    dll_insert_at_tail(list, dll_new_node(node));
    curr_context = curr_context->parent;
  }
  return list;
}

Context *context_LCA(Context *context_A, Context *context_B);

void context_free(Context *context) { 
  free(context);
}

Context *context_add(Context *context_A, Context *context_B) {
  if (context_A == context_B) {
    return context_A;
  }

  DoublyLinkedList *A_ancestors = context_ancestors(context_A);
  int n = dll_len(A_ancestors);

  Context *result = context_B;
  for (int i = 0; i < n; i++) {
    Context *curr_A_ctx = dll_at(A_ancestors, i)->data;
    Expression *curr_A_expr = curr_A_ctx->var_type;
    if (context_find(context_B, curr_A_expr) == NULL) {
      result = context_insert(result, curr_A_expr);
    }
  }

  return result;
}

// Suppose context has the form [variable1: type1] ... [subtrahend:
// subtrahendtype]... Then this function returns the context up until, but not
// including, the subtrahend, or the original context if the subtrahend is not
// found. Theoretically, one does not need to be so restrictive and can instead
// check what future context nodes actually rely on the subtrahend, but this is
// not what this function's purpose is.
Context *context_minus(Context *context, Expression *subtrahend) {
  if (context_find(context, subtrahend) == NULL) {
    return context;
  }

  Context *curr_ctx = context;
  while (curr_ctx->var_type != subtrahend) {
    curr_ctx = curr_ctx->parent;
  }

  return curr_ctx->parent;
}

bool valid_in_context(Expression *expr, Context *context) {
  Context *expr_ctx = get_expression_context(expr);
  Context *curr_e_ctx = expr_ctx;
  while (!context_is_empty(curr_e_ctx)) {
    Expression *curr_var = curr_e_ctx->var_type;
    if (context_find(context, curr_var) == NULL) {
      return false;
    }
    curr_e_ctx = curr_e_ctx->parent;
  }
  return true;
}
