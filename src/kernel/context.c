#include "src/kernel/context.h"

#include "src/kernel/expression.h"
#include "src/kernel/new_subst.h"

Context *context_create_empty() {
    if (EMPTY_CONTEXT == NULL) {
        EMPTY_CONTEXT = calloc(1, sizeof(Context));
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

    Expression *curr_ctxB = contextB;
    while (!context_is_empty(curr_ctxB)) {
        if (contextA == curr_ctxB) {
            return true;
        }
        curr_ctxB = get_expression_context(curr_ctxB);
    }

    return false;
}

Expression *context_find(Expression *context, Expression *var) {
    Expression *curr = context;
    while (!context_is_empty(curr)) {
        if (curr == var) {
            return curr;
        }

        curr = get_expression_context(curr);
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

DoublyLinkedList *context_ancestors_until(Expression *context_A, Expression *until) {
    DoublyLinkedList *list = dll_create();
    Expression *curr_context = context_A;
    while (!context_is_empty(curr_context) && curr_context != until) {
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

    char *new_var_name = malloc(strlen(old_var_name) + 2);
    strcpy(new_var_name, old_var_name);
    strcat(new_var_name, "'");

    // new_parent is already the "cut" context, so call _subst directly
    Expression *new_var_type = _subst(new_parent, old_var_type, x, a);

    Expression *new_var = init_var_expression_wc(new_var_name, new_var_type, new_parent);
    free(new_var_name);
    return new_var;
}

bool valid_in_context(Expression *expr, Expression *context) {
    Expression *curr_expr_ctx = get_expression_context(expr);
    if (context_is_ancestor(curr_expr_ctx, context)) {
        return true;
    }

    while (!context_is_empty(curr_expr_ctx)) {
        Expression *curr_var = curr_expr_ctx;
        if (context_find(context, curr_var) == NULL) {
            return false;
        }
        curr_expr_ctx = get_expression_context(curr_expr_ctx);
    }
    return true;
}

bool valid_to_add_to_context(Expression *expr, Expression *context) {
    if (expr->tag != VAR_EXPRESSION) {
        return false;
    }

    Expression *expr_type = get_expression_type(expr);
    return valid_in_context(expr_type, context);
}
