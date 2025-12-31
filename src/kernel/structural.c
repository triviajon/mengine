#include "src/kernel/structural.h"

#include <stdlib.h>

#include "src/kernel/dyn_array_map.h"
#include "src/kernel/expression.h"
#include "src/kernel/inductive.h"

// Check if a term is in constructor form
static bool is_constructor_form(Expression *expr) {
    Expression *head = get_head(expr);
    return is_constructor(head);
}

static bool is_prop(Expression *expr) { return expr->tag == PROP_EXPRESSION; }

// Extract constructor arguments from a constructor form
static Expression **get_constructor_args(Expression *expr, int *arg_count) {
    if (!is_constructor_form(expr)) {
        *arg_count = 0;
        return NULL;
    }

    // Collect arguments by traversing the application spine
    DoublyLinkedList *args = dll_create();
    Expression *curr = expr;

    while (curr->tag == APP_EXPRESSION) {
        dll_insert_at_head(args, dll_new_node(get_app_arg(curr)));
        curr = get_app_func(curr);
    }

    *arg_count = dll_len(args);
    Expression **result = malloc(*arg_count * sizeof(Expression *));
    for (int i = 0; i < *arg_count; i++) {
        result[i] = dll_at(args, i)->data;
    }
    dll_destroy(args);
    return result;
}

// Check if expr is an instance of base (i.e., expr = base v1 ... vm)
static bool is_instance_of(Expression *expr, Expression *base) {
    Expression *expr_head = get_head(expr);

    return congruence(expr_head, base);
}

static bool _structurally_smaller_than_arg(Expression *term, Expression *arg, Map *visited) {
    if (map_get(visited, arg)) {
        return false;
    }

    bool *marker = malloc(sizeof(bool));
    *marker = true;
    map_set(visited, arg, marker);

    if (term_directly_structurally_smaller_than_arg(term, arg)) {
        return true;
    }

    int arg_count;
    Expression **ctor_args = get_constructor_args(arg, &arg_count);
    if (ctor_args == NULL) {
        return false;
    }

    for (int i = 0; i < arg_count; i++) {
        if (_structurally_smaller_than_arg(term, ctor_args[i], visited)) {
            free(ctor_args);
            return true;
        }
    }

    free(ctor_args);
    return false;
}

bool term_directly_structurally_smaller_than_arg(Expression *term, Expression *arg) {
    // Case 1: Both are Props and arity 0
    Expression *term_type = get_expression_type(term);
    Expression *arg_type = get_expression_type(arg);

    if (is_prop(term_type) && is_prop(arg_type) && get_arity(term) == 0 && get_arity(arg) == 0) {
        return true;
    }

    // Case 2: arg is in constructor form c(a1, ..., an)
    int arg_count;
    Expression **ctor_args = get_constructor_args(arg, &arg_count);
    if (ctor_args == NULL) {
        return false;
    }

    for (int i = 0; i < arg_count; i++) {
        Expression *aj = ctor_args[i];
        int aj_arity = get_arity(aj);

        if (aj_arity == 0) {
            if (congruence(term, aj)) {
                free(ctor_args);
                return true;
            }
        } else {
            if (is_instance_of(term, aj)) {
                free(ctor_args);
                return true;
            }
        }
    }

    free(ctor_args);
    return false;
}

bool term_structurally_smaller_than_arg(Expression *term, Expression *arg) {
    Map *visited = map_new();
    bool result = _structurally_smaller_than_arg(term, arg, visited);

    map_clear_free(visited);

    return result;
}
