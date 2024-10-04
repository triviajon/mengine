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

void context_free(Context *context) {
    return;
}

Expression *context_head(Context *context) {
    return NULL; // TODO
}

Context *context_tail(Context *context) {
    return context->parent;
}
