#include "context.h"

Context *context_create_empty() {
  if (EMPTY_CONTEXT == NULL) {
    EMPTY_CONTEXT = (Context *)malloc(sizeof(Context));
    EMPTY_CONTEXT->variable = NULL;
    EMPTY_CONTEXT->type = NULL;
    EMPTY_CONTEXT->parent = NULL;
  }
  return EMPTY_CONTEXT;
}

bool context_is_empty(Context *context) {
    return context->parent == NULL;
}

Context *context_insert(Context *context, Expression *var, Expression *type) {
    Context *new_ctx = (Context *)malloc(sizeof(Context));
    new_ctx->parent = context;
    new_ctx->variable = var;
    new_ctx->type = type;
    return new_ctx;
}

Expression *context_lookup(Context *context, Expression *var) {
    if (context_is_empty(context)) {
        return NULL; 
    } else if (context->variable == var) {
        return context->type;
    } else {
        return context_lookup(context->parent, var);
    }
}

bool *context_is_ancestor(Context *context_A, Context *context_B) {
    // context_A is an ancestor of context_B if at some point traveling the context_B parent chain,
    // we encounter context_A. 
    Context *curr_B = context_B;
    while (!context_is_empty(curr_B)) {
        if (context_A == curr_B) {
            return true;
        }
        curr_B = curr_B->parent;
    }
    return false;
}

void context_free(Context *context) {
    return;
}

// Expression *context_head(Context *context) {
//     return NULL; // TODO maybe
// }

Context *context_tail(Context *context) {
    return context->parent;
}
