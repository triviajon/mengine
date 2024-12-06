#include "context.h"

#include "beta_reduction.h"
#include "expression.h"

Context *context_create_empty() {
  if (EMPTY_CONTEXT == NULL) {
    EMPTY_CONTEXT = (Context *)malloc(sizeof(Context));
    EMPTY_CONTEXT->variable = NULL;
    EMPTY_CONTEXT->type = NULL;
    EMPTY_CONTEXT->parent = NULL;
  }
  return EMPTY_CONTEXT;
}

bool context_is_empty(Context *context) { return context->parent == NULL; }

Context *handle_bad_context() {
  printf("Bad context creation.\n");
  return NULL;
}

Context *context_insert(Context *context, Expression *var, Expression *type) {
  Context *new_ctx = (Context *)malloc(sizeof(Context));
  new_ctx->parent = context;
  new_ctx->variable = var;
  // var->value.var.context = new_ctx;
  // Context *type_context = get_expression_context(type);
  // if (!context_is_empty(type_context) &&
  //     !context_is_ancestor(type_context, new_ctx)) {
  //   return handle_bad_context();
  // }
  new_ctx->type = type;
  return new_ctx;
}

int context_size(Context *context) {
  int size = 0;
  Context *curr = context;
  while (!context_is_empty(context)) {
    curr = curr->parent;
    size++;
  }
  return size;
}

Context *context_find(Context *context, Expression *var) {
  if (context_is_empty(context)) {
    return NULL;
  } else if (context->variable == var) {
    return context;
  } else {
    return context_find(context->parent, var);
  }
}

Expression *context_lookup(Context *context, Expression *var) {
  Context *defining_ctx = context_find(context, var);
  if (defining_ctx != NULL) {
    return defining_ctx->type;
  }
  return NULL;
}

bool context_is_ancestor(Context *context_A, Context *context_B) {
  // context_A is an ancestor of context_B if at some point traveling the
  // context_B parent chain, we encounter context_A.
  Context *curr_B = context_B;
  while (!context_is_empty(curr_B)) {
    if (context_A == curr_B) {
      return true;
    }
    curr_B = curr_B->parent;
  }
  return false;
}

DoublyLinkedList *context_ancestors(Context *context_A) {
  DoublyLinkedList *list_A = dll_create();

  for (Context *current = context_A; current != NULL;
       current = current->parent) {
    dll_insert_at_head(list_A, dll_new_node(current));
  }

  return list_A;
}

Context *context_LCA(Context *context_A, Context *context_B) {
  // Ancestors of context_A and context_B
  DoublyLinkedList *list_A = dll_create();
  DoublyLinkedList *list_B = dll_create();

  for (Context *current = context_A; current != NULL;
       current = current->parent) {
    dll_insert_at_head(list_A, dll_new_node(current));
  }

  for (Context *current = context_B; current != NULL;
       current = current->parent) {
    dll_insert_at_head(list_B, dll_new_node(current));
  }

  DLLNode *node_A = list_A->head;
  DLLNode *node_B = list_B->head;
  Context *last_common_ancestor = NULL;

  while (node_A != NULL && node_B != NULL) {
    if (node_A->data == node_B->data) {
      last_common_ancestor = (Context *)node_A->data;
      node_A = node_A->next;
      node_B = node_B->next;
    } else {
      break;
    }
  }

  dll_destroy(list_A);
  dll_destroy(list_B);

  return last_common_ancestor;
}

void context_free(Context *context) { return; }

/*
  This function combines two context trees by finding their lowest common
  ancestor (LCA), and appends the necessary context nodes with appropriate
  substitutions to avoid redundancy and preserve context tree integrity.
*/
Context *context_combine(Context *body_context, Expression *old, Expression *old_ty, Expression *new) {
  if (new->type == VAR_EXPRESSION || new->type == HOLE_EXPRESSION) {
    // Then the returned context needs to contain new: old_ty, and anything required
    // for old_ty to be valid. 
    Context *old_defn = context_find(body_context, old);
    if (old_defn == NULL) {
      return body_context;
    }

    DoublyLinkedList *body_context_listed = context_ancestors(body_context);

    DLLNode *curr = body_context_listed->head;
    while (curr->data != old_defn) {
      curr = curr->next;
    }
    curr = curr->next;

    // Instead of old: old_ty, say new: old_ty.
    Context *final_context = context_insert(old_defn->parent, new, old_ty);

    // Then start adding in the rest of the context nodes, with proper
    // replacements.
    while (curr != NULL) {
      Context *body_ctx_node = (Context *)curr->data;
      Expression *body_ctx_node_type =
          context_lookup(body_ctx_node, old) == NULL
              ? body_ctx_node->type
              : reduce_body(body_ctx_node->type, old, old_ty, new);
      final_context = context_insert(final_context, body_ctx_node->variable,
                                    body_ctx_node_type);
      curr = curr->next;
    }

    return final_context;
  }

  Context *new_context = get_expression_context(new);
  Context *lca_of_ctx = context_LCA(body_context, new_context);

  DoublyLinkedList *body_context_listed = context_ancestors(body_context);

  DLLNode *curr = body_context_listed->head;
  // Skip ahead to LCA in body_context_listed
  while (curr->data != lca_of_ctx) {
    curr = curr->next;
  }

  curr = curr->next;  // Start at the next after LCA.
  Context *final_context =
      new_context;  // Start from the context needed to define new.

  // Then start adding in the rest of the context nodes, with proper
  // replacements.
  while (curr != NULL) {
    Context *body_ctx_node = (Context *)curr->data;
    Expression *body_ctx_node_type =
        context_lookup(body_ctx_node, old) == NULL
            ? body_ctx_node->type
            : reduce_body(body_ctx_node->type, old, old_ty, new);
    final_context = context_insert(final_context, body_ctx_node->variable,
                                   body_ctx_node_type);
    curr = curr->next;
  }

  return final_context;
}

Expression *get_binding_variable_type(Context *context) {
  return context->type;
}

Expression *get_binding_variable(Context *context) { return context->variable; }

Context *context_tail(Context *context) { return context->parent; }

bool type_valid(Context *context, Expression *type) {
  Context *type_context = get_expression_context(type);
  return context_is_empty(type_context) ||
         context_is_ancestor(type_context, context);
}
