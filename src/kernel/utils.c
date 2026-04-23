#include "src/kernel/utils.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/common/linear_map.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"

// Helper function to concatenate two strings
char *str_concat(const char *s1, const char *s2) {
    char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

// Like str_concat but frees s1 (use when s1 is a heap-allocated intermediate)
static char *str_concat_free(char *s1, const char *s2) {
    char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    free(s1);
    return result;
}

// Helper function to add parentheses around an expression if needed
char *parenthesize_and_free(char *expr_str) {
    char *result = (char *)malloc(strlen(expr_str) + 3);  // +3 for '(', ')' and '\0'
    strcpy(result, "(");
    strcat(result, expr_str);
    strcat(result, ")");
    free(expr_str);  // Free the original string
    return result;
}

char *stringify_expression(Expression *expression) {
    char *result = NULL;

    switch (expression->tag) {
        case VAR_EXPRESSION:
            result = strdup(get_var_name(expression));
            break;

        case LAMBDA_EXPRESSION: {
            char *var_str = stringify_expression(get_lambda_bound_variable(expression));
            char *type_str =
                stringify_expression(get_expression_type(get_lambda_bound_variable(expression)));
            char *body_str = stringify_expression(get_lambda_body(expression));
            result = str_concat("fun (", var_str);
            result = str_concat_free(result, ": ");
            result = str_concat_free(result, type_str);
            result = str_concat_free(result, ") => ");
            result = str_concat_free(result, body_str);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }
        case APP_EXPRESSION: {
            char *func_str = stringify_expression(get_app_func(expression));
            char *arg_str = stringify_expression(get_app_arg(expression));

            char *app_str = str_concat(func_str, " ");
            app_str = str_concat_free(app_str, arg_str);

            result = parenthesize_and_free(app_str);
            free(func_str);
            free(arg_str);
            break;
        }

        case FORALL_EXPRESSION: {
            char *var_str = stringify_expression(get_forall_bound_variable(expression));
            char *type_str =
                stringify_expression(get_expression_type(get_forall_bound_variable(expression)));
            char *body_str = stringify_expression(get_forall_body(expression));
            result = str_concat("forall (", var_str);
            result = str_concat_free(result, ": ");
            result = str_concat_free(result, type_str);
            result = str_concat_free(result, "), ");
            result = str_concat_free(result, body_str);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }

        case MATCH_EXPRESSION: {
            char *scrutinee_str = stringify_expression(expression->as.match.scrutinee);
            result = str_concat("match ", scrutinee_str);
            result = str_concat_free(result, " with");
            free(scrutinee_str);

            for (int i = 0; i < expression->as.match.branch_count; i++) {
                MatchBranch *branch = expression->as.match.branches[i];
                result = str_concat_free(result, " | ");

                char *ctor_str = stringify_expression(branch->constructor);
                result = str_concat_free(result, ctor_str);
                free(ctor_str);

                for (int j = 0; j < branch->pattern_var_count; j++) {
                    result = str_concat_free(result, " ");
                    char *var_str = stringify_expression(branch->pattern_variables[j]);
                    result = str_concat_free(result, var_str);
                    free(var_str);
                }

                result = str_concat_free(result, " => ");
                char *body_str = stringify_expression(branch->body);
                result = str_concat_free(result, body_str);
                free(body_str);
            }

            result = str_concat_free(result, " end");
            break;
        }

        case FIX_EXPRESSION: {
            char *rec_var_str = stringify_expression(expression->as.fix.recursive_var);
            result = str_concat("fix ", rec_var_str);
            free(rec_var_str);

            for (int i = 0; i < expression->as.fix.arg_count; i++) {
                result = str_concat_free(result, " (");
                char *arg_str = stringify_expression(expression->as.fix.args[i]);
                result = str_concat_free(result, arg_str);
                result = str_concat_free(result, ": ");
                char *arg_type_str =
                    stringify_expression(get_expression_type(expression->as.fix.args[i]));
                result = str_concat_free(result, arg_type_str);
                result = str_concat_free(result, ")");
                free(arg_str);
                free(arg_type_str);
            }

            result = str_concat_free(result, " {struct ");
            char *decreasing_arg_str = stringify_expression(
                expression->as.fix.args[expression->as.fix.decreasing_arg_index]);
            result = str_concat_free(result, decreasing_arg_str);
            result = str_concat_free(result, "}");
            free(decreasing_arg_str);

            result = str_concat_free(result, " : ");
            char *return_type_str =
                stringify_expression(get_expression_type(expression->as.fix.body));
            result = str_concat_free(result, return_type_str);
            free(return_type_str);

            result = str_concat_free(result, " := ");
            char *body_str = stringify_expression(expression->as.fix.body);
            result = str_concat_free(result, body_str);
            free(body_str);
            break;
        }

        case TYPE_EXPRESSION:
            result = strdup("Type");
            break;

        case PROP_EXPRESSION:
            result = strdup("Prop");
            break;

        case HOLE_EXPRESSION: {
            char *hole_name = strdup(get_hole_name(expression));
            result = str_concat("?", hole_name);
            free(hole_name);
            break;
        }
    }

    return result;
}

char *stringify_context(Context *context, ContextStringifyOptions opts) {
    if (!context || context_is_empty(context)) {
        return strdup("");
    }

    Expression *var = context;
    char *var_str = stringify_expression(var);
    char *type_str = stringify_expression(get_expression_type(var));
    char *parent_str = stringify_context(get_expression_context(var), opts);

    char *result = parent_str;

    // Handle indent option
    if (opts.indent > 0) {
        for (int i = 0; i < opts.indent; i++) {
            result = str_concat_free(result, " ");
        }
    }

    // Handle print_prefix option
    if (opts.print_prefix) {
        result = str_concat_free(result, "Variable ");
    }

    result = str_concat_free(result, var_str);
    result = str_concat_free(result, " : ");
    result = str_concat_free(result, type_str);
    result = str_concat_free(result, ".\n");

    free(var_str);
    free(type_str);
    // parent_str was consumed by the first str_concat_free call - do not free again

    return result;
}

char *stringify_context_until(Context *context, Context *until, ContextStringifyOptions opts) {
    if (context == until || context_is_empty(context)) {
        return strdup("");
    }

    Expression *var = context;
    char *var_str = stringify_expression(var);
    char *type_str = stringify_expression(get_expression_type(var));
    char *parent_str = stringify_context_until(get_expression_context(var), until, opts);

    char *result = parent_str;

    // Handle indent option
    if (opts.indent > 0) {
        for (int i = 0; i < opts.indent; i++) {
            result = str_concat_free(result, " ");
        }
    }

    // Handle print_prefix option
    if (opts.print_prefix) {
        result = str_concat_free(result, "Variable ");
    }

    result = str_concat_free(result, var_str);
    result = str_concat_free(result, " : ");
    result = str_concat_free(result, type_str);
    result = str_concat_free(result, ".\n");

    free(var_str);
    free(type_str);
    // parent_str was consumed by the first str_concat_free call - do not free again

    return result;
}

char *_get_str_addr(Expression *expression) {
    char buf[(2 * sizeof(void *)) + 1];
    snprintf(buf, sizeof(buf), "%p", (void *)expression);
    return strdup(str_concat("var", buf));
}

char *_top_level_stringify_to_address(Expression *expression) {
    char *result = NULL;

    switch (expression->tag) {
        case VAR_EXPRESSION: {
            char buf[(2 * sizeof(void *)) + 1];
            snprintf(buf, sizeof(buf), "%p", (void *)expression);
            result = strdup(str_concat("var", buf));
            break;
        }

        case APP_EXPRESSION: {
            char buf1[(2 * sizeof(void *)) + 1];
            snprintf(buf1, sizeof(buf1), "%p", (void *)get_app_func(expression));
            result = strdup(str_concat("var", buf1));

            char buf2[(2 * sizeof(void *)) + 1];
            snprintf(buf2, sizeof(buf2), "%p", (void *)get_app_arg(expression));
            result = strdup(str_concat(str_concat(result, " "), str_concat("var", buf2)));
            result = parenthesize_and_free(result);
            break;
        }

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

void _count_subexpression(Expression *expression, LinearMap *visited, LinearMap *counts) {
    if (linear_map_get(visited, expression)) {
        return;
    }
    bool *have_visited = malloc(sizeof(bool));

    *have_visited = true;  // truly, this can be any non-zero junk. TODO:

    // make this a set

    linear_map_set(visited, expression, have_visited);

    switch (expression->tag) {
        case VAR_EXPRESSION:
            break;
        case APP_EXPRESSION: {
            int *func_lookup_value = linear_map_get(counts, get_app_func(expression));
            if (!func_lookup_value) {
                int *value = malloc(sizeof(int));
                *value = 1;
                linear_map_set(counts, (void *)get_app_func(expression), value);
            } else {
                (*func_lookup_value)++;
            }

            int *arg_lookup_value = linear_map_get(counts, get_app_arg(expression));
            if (!arg_lookup_value) {
                int *value = malloc(sizeof(int));
                *value = 1;
                linear_map_set(counts, (void *)get_app_arg(expression), value);
            } else {
                (*arg_lookup_value)++;
            }

            _count_subexpression(get_app_func(expression), visited, counts);
            _count_subexpression(get_app_arg(expression), visited, counts);
            break;
        }

        default:
            break;
    }
}

DoublyLinkedList *topo_order(Expression *top_expr, LinearMap *expr_counts) {
    // We assume top_expr is NOT in expr_counts, since it should always have
    // in-degree 0.

    DoublyLinkedList *L = dll_create();  // Empty list that will contain the sorted elements
    DoublyLinkedList *S = dll_create();  // Set of all nodes with no incoming edge
    dll_insert_at_head(S, dll_new_node(top_expr));

    while (S->head != NULL) {
        DLLNode *n = dll_remove_head(S);
        dll_insert_at_tail(L, n);

        Expression *expr = (Expression *)n->data;

        // Reduce counts for all of expr's immediate children
        switch (expr->tag) {
            case (VAR_EXPRESSION):
                break;
            case (APP_EXPRESSION): {
                Expression *func = get_app_func(expr);
                int *func_count = linear_map_get(expr_counts, func);
                (*func_count)--;

                Expression *arg = get_app_arg(expr);
                int *arg_count = linear_map_get(expr_counts, arg);
                (*arg_count)--;
                break;
            }
            default:
                break;
        }

        // For each of expr's immediate children who have a count of 0, add them
        // to S.
        switch (expr->tag) {
            case (VAR_EXPRESSION):
                break;
            case (APP_EXPRESSION): {
                Expression *func = get_app_func(expr);
                int *func_count = linear_map_get(expr_counts, func);
                if (*func_count == 0) {
                    dll_insert_at_tail(S, dll_new_node(func));
                }
                Expression *arg = get_app_arg(expr);
                int *arg_count = linear_map_get(expr_counts, arg);
                if (*arg_count == 0) {
                    dll_insert_at_tail(S, dll_new_node(arg));
                }
                break;
            }
            default:
                break;
        }
    }

    free(S);
    return L;
}

char *se(Expression *expression) { return stringify_expression(expression); }

char *sc(Context *context) { return stringify_context(context, CTX_STRINGIFY_VERBOSE); }

char *_stringify_expression_with_let(Expression *expression) {
    char *result = NULL;

    switch (expression->tag) {
        case VAR_EXPRESSION:
            result = strdup(get_var_name(expression));
            break;

        case LAMBDA_EXPRESSION: {
            if (get_expression_uplinks(expression)->head != NULL && expression->uplink_count > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *var_str = _stringify_expression_with_let(

                get_lambda_bound_variable(expression));

            char *type_str = _stringify_expression_with_let(

                get_expression_type(get_lambda_bound_variable(expression)));

            char *body_str = _stringify_expression_with_let(

                get_lambda_body(expression));

            result = str_concat("fun (", var_str);

            result = str_concat(result, ": ");

            result = str_concat(result, type_str);

            result = str_concat(result, ") => ");

            result = str_concat(result, body_str);

            result = parenthesize_and_free(result);

            free(var_str);

            free(type_str);

            free(body_str);

            break;
        }
        case APP_EXPRESSION: {
            if (get_expression_uplinks(expression)->head != NULL && expression->uplink_count > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *func_str =

                _stringify_expression_with_let(get_app_func(expression));

            char *arg_str =

                _stringify_expression_with_let(get_app_arg(expression));

            char *app_str = str_concat(func_str, " ");

            app_str = str_concat(app_str, arg_str);

            result = parenthesize_and_free(app_str);

            free(func_str);

            free(arg_str);

            break;
        }

        case FORALL_EXPRESSION: {
            if (get_expression_uplinks(expression)->head != NULL && expression->uplink_count > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *var_str = _stringify_expression_with_let(

                get_forall_bound_variable(expression));

            char *type_str = _stringify_expression_with_let(

                get_expression_type(get_forall_bound_variable(expression)));

            char *body_str = _stringify_expression_with_let(

                get_forall_body(expression));

            result = str_concat("forall (", var_str);

            result = str_concat(result, ": ");

            result = str_concat(result, type_str);

            result = str_concat(result, "), ");

            result = str_concat(result, body_str);

            result = parenthesize_and_free(result);

            free(var_str);

            free(type_str);

            free(body_str);

            break;
        }

        case TYPE_EXPRESSION:
            result = strdup("Type");
            break;

        case PROP_EXPRESSION:
            result = strdup("Prop");
            break;

        case HOLE_EXPRESSION:
            result = str_concat("?", strdup(get_hole_name(expression)));
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

char *_top_level_stringify_expression_with_let(Expression *expression) {
    char *result = NULL;

    switch (expression->tag) {
        case VAR_EXPRESSION:
            result = strdup(get_var_name(expression));
            break;

        case LAMBDA_EXPRESSION: {
            char *var_str = _stringify_expression_with_let(get_lambda_bound_variable(expression));
            char *type_str = _stringify_expression_with_let(
                get_expression_type(get_lambda_bound_variable(expression)));
            char *body_str = _stringify_expression_with_let(get_lambda_body(expression));
            result = str_concat("fun (", var_str);
            result = str_concat(result, ": ");
            result = str_concat(result, type_str);
            result = str_concat(result, ") => ");
            result = str_concat(result, body_str);
            result = parenthesize_and_free(result);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }
        case APP_EXPRESSION: {
            char *func_str = _stringify_expression_with_let(get_app_func(expression));
            char *arg_str = _stringify_expression_with_let(get_app_arg(expression));

            char *app_str = str_concat(func_str, " ");
            app_str = str_concat(app_str, arg_str);

            result = parenthesize_and_free(app_str);
            free(func_str);
            free(arg_str);
            break;
        }

        case FORALL_EXPRESSION: {
            char *var_str = _stringify_expression_with_let(get_forall_bound_variable(expression));
            char *type_str = _stringify_expression_with_let(
                get_expression_type(get_forall_bound_variable(expression)));
            char *body_str = _stringify_expression_with_let(get_forall_body(expression));
            result = str_concat("forall (", var_str);
            result = str_concat(result, ": ");
            result = str_concat(result, type_str);
            result = str_concat(result, "), ");
            result = str_concat(result, body_str);
            result = parenthesize_and_free(result);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }

        case TYPE_EXPRESSION:
            result = strdup("Type");
            break;

        case PROP_EXPRESSION:
            result = strdup("Prop");
            break;

        case HOLE_EXPRESSION:
            result = str_concat("?", strdup(get_hole_name(expression)));
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

char *stringify_expression_with_let(Expression *expression) {
    LinearMap *visited = linear_map_new();  // maps addresses to visited bool
    LinearMap *counts = linear_map_new();   // maps addresses to counts
    _count_subexpression(expression, visited, counts);
    DoublyLinkedList *ordering = topo_order(expression, counts);

    char *result = NULL;
    for (int i = dll_len(ordering) - 1; i >= 0; i--) {
        DLLNode *node = dll_at(ordering, i);
        Expression *node_expr = (Expression *)node->data;

        if (node_expr->tag != VAR_EXPRESSION && node_expr->uplink_count > 1) {
            char *str_adr = _get_str_addr(node_expr);
            char *expr_string = _top_level_stringify_expression_with_let(node_expr);
            char *line = str_concat("let ", str_adr);
            line = str_concat(line, " := ");
            line = str_concat(line, expr_string);
            line = str_concat(line, " in\n");

            if (result != NULL) {
                char *temp = str_concat(result, line);
                free(result);
                free(line);
                result = temp;
            } else {
                result = line;
            }
        }
    }

    char *stringified_expr = _stringify_expression_with_let(expression);
    char *final_output = (result == NULL) ? stringified_expr : str_concat(result, stringified_expr);
    return final_output;
}