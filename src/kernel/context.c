#include "src/kernel/context.h"

#include <stdlib.h>

#include "src/kernel/expression.h"
#include "src/kernel/order.h"
#include "src/kernel/subst.h"

Context *context_create_empty() {
    if (EMPTY_CONTEXT == NULL) {
        EMPTY_CONTEXT = calloc(1, sizeof(Context));
#ifndef ORDER_USE_LL
        EMPTY_CONTEXT->order_out.tag = UINT64_MAX;
#endif
    }
    return EMPTY_CONTEXT;
}

bool context_is_empty(Expression *context) { return context == EMPTY_CONTEXT; }

bool context_contains_name(Expression *context, char *name) {
    Expression *curr = context;
    while (!context_is_empty(curr)) {
        if (strcmp(get_var_name(curr), name) == 0) {
            return true;
        }
        curr = get_expression_context(curr);
    }
    return false;
}

Expression *context_lookup_by_name(Expression *context, char *name) {
    Expression *curr = context;
    while (!context_is_empty(curr)) {
        if (strcmp(get_var_name(curr), name) == 0) {
            return curr;
        }
        curr = get_expression_context(curr);
    }
    return NULL;
}

bool context_is_ancestor(Expression *contextA, Expression *contextB) {
    if (contextA == contextB) {
        return true;
    }
    if (context_is_empty(contextA)) {
        return true;
    }
    if (context_is_empty(contextB)) {
        return false;
    }
    return order_precedes(contextA, contextB);
}

Expression *context_find(Expression *context, Expression *var) {
    if (context_is_empty(var)) {
        return NULL;
    }
    if (order_precedes(var, context) || var == context) {
        return var;
    }
    return NULL;
}

DoublyLinkedList *context_ancestors(Expression *context_A) {
    DoublyLinkedList *list = dll_create();
    Expression *curr_context = context_A;
    while (!context_is_empty(curr_context)) {
        dll_insert_at_head(list, dll_new_node(curr_context));
        curr_context = get_expression_context(curr_context);
    }
    return list;
}

void context_free(Expression *context) { free(context); }

Context *context_cut(Context *context, Expression *x, Expression *a) {
    // First, let's make sure x occurs in context.
    if (context_find(context, x) == NULL) {
        return context;
    }

    Context *new_context = context_replace(context, x, a);

    return new_context;
}

Context *context_replace(Context *context, Expression *x, Expression *a) {
    Context *parent = get_expression_context(context);

    if (context == x) {
        // Once we've reached the x context, we know that everything after it has been replaced
        // with a. Thus, we can get rid of the x binding.
        return parent;
    }

    Context *new_parent = context_replace(parent, x, a);
    char *old_var_name = get_var_name(context);
    Expression *old_var_type = get_expression_type(context);

    char *new_var_name = strdup(old_var_name);
    // new_parent is already the "cut" context, so call _subst directly
    Expression *new_var_type = _subst(new_parent, old_var_type, x, a);

    Expression *new_var = init_var_expression_wc(new_var_name, new_var_type, new_parent);
    free(new_var_name);
    return new_var;
}

bool valid_in_context(Expression *expr, Expression *context) {
    Expression *curr_expr_ctx = get_expression_context(expr);
    return context_is_ancestor(curr_expr_ctx, context);
}

int context_size(Context *context) {
    if (context_is_empty(context)) {
        return 0;
    }
    return context->ctx_size;
}

bool valid_to_add_to_context(Expression *expr, Expression *context) {
    if (expr->tag != VAR_EXPRESSION) {
        return false;
    }

    Expression *expr_type = get_expression_type(expr);
    return valid_in_context(expr_type, context);
}
