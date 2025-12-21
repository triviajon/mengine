#include "src/kernel/utils.h"

#include <stdio.h>

#include "src/kernel/context.h"
#include "src/kernel/dyn_array_map.h"

// Helper function to concatenate two strings
char *str_concat(const char *s1, const char *s2) {
    char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

// Helper function to add parentheses around an expression if needed
char *parenthesize_and_free(char *expr_str) {
    char *result =
        (char *)malloc(strlen(expr_str) + 3);  // +3 for '(', ')' and '\0'
    strcpy(result, "(");
    strcat(result, expr_str);
    strcat(result, ")");
    free(expr_str);  // Free the original string
    return result;
}

char *stringify_expression(Expression *expression) {
    char *result = NULL;

    switch (expression->type) {
        case VAR_EXPRESSION:
            result = strdup(expression->value.var.name);
            break;

        case LAMBDA_EXPRESSION: {
            char *var_str =
                stringify_expression(expression->value.lambda.bound_variable);
            char *type_str = stringify_expression(
                expression->value.lambda.bound_variable->value.var.type);
            char *body_str =
                stringify_expression(expression->value.lambda.body);
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
            char *func_str = stringify_expression(expression->value.app.func);
            char *arg_str = stringify_expression(expression->value.app.arg);

            char *app_str = str_concat(func_str, " ");
            app_str = str_concat(app_str, arg_str);

            result = parenthesize_and_free(app_str);
            free(func_str);
            free(arg_str);
            break;
        }

        case FORALL_EXPRESSION: {
            char *var_str =
                stringify_expression(expression->value.forall.bound_variable);
            char *type_str = stringify_expression(
                expression->value.forall.bound_variable->value.var.type);
            char *body_str =
                stringify_expression(expression->value.forall.body);
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
            result = str_concat("?", strdup(expression->value.hole.name));
            break;

        case FIX_EXPRESSION: {
            char *ident_str = stringify_expression(expression->value.fix.ident);
            char *var_str =
                stringify_expression(expression->value.fix.bound_variable);
            char *type_str = stringify_expression(
                expression->value.fix.bound_variable->value.var.type);
            char *body_str = stringify_expression(expression->value.fix.body);
            result = str_concat("(fix ", ident_str);
            result = str_concat(result, " (");
            result = str_concat(result, var_str);
            result = str_concat(result, ": ");
            result = str_concat(result, type_str);
            result = str_concat(result, ") := ");
            result = str_concat(result, body_str);
            result = str_concat(result, ")");
            free(ident_str);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }

        case MATCH_EXPR_EXPRESSION: {
            char *match_scrutinee_str = stringify_expression(
                expression->value.matchExpr.match_scrutinee);
            char *literal_case_item_str = stringify_expression(
                expression->value.matchExpr.literal_case_item);
            char *literal_result_str = stringify_expression(
                expression->value.matchExpr.literal_result);
            char *var_case_item_str =
                stringify_expression(expression->value.matchExpr.var_case_item);
            char *var_result_str =
                stringify_expression(expression->value.matchExpr.var_result);
            char *op_case_item_str =
                stringify_expression(expression->value.matchExpr.op_case_item);
            char *op_result_str =
                stringify_expression(expression->value.matchExpr.op_result);
            result = str_concat("match ", match_scrutinee_str);
            result = str_concat(result, " with ");

            result = str_concat(result, "| ");
            result = str_concat(result, literal_case_item_str);
            result = str_concat(result, " => ");
            result = str_concat(result, literal_result_str);
            result = str_concat(result, " ");

            result = str_concat(result, "| ");
            result = str_concat(result, var_case_item_str);
            result = str_concat(result, " => ");
            result = str_concat(result, var_result_str);
            result = str_concat(result, " ");

            result = str_concat(result, "| ");
            result = str_concat(result, op_case_item_str);
            result = str_concat(result, " => ");
            result = str_concat(result, op_result_str);
            result = str_concat(result, " ");

            result = str_concat(result, "end");
            break;
        }
    }

    return result;
}

char *stringify_context(Context *context) {
    if (context_is_empty(context)) {
        return strdup("");
    }
    char *var_str = stringify_expression(context->var_type);
    char *type_str = stringify_expression(context->var_type->value.var.type);
    char *parent_str = stringify_context(context->parent);

    char *result = str_concat(parent_str, "\n");
    result = str_concat(result, "Variable ");
    result = str_concat(result, var_str);
    result = str_concat(result, " : ");
    result = str_concat(result, type_str);
    result = str_concat(result, ".");

    free(var_str);
    free(type_str);
    free(parent_str);

    return result;
}

char *stringify_context_until(Context *context, Context *until) {
    if (context == until || context_is_empty(context)) {
        return strdup("");
    }

    char *var_str = stringify_expression(context->var_type);
    char *type_str = stringify_expression(context->var_type->value.var.type);
    char *parent_str = stringify_context_until(context->parent, until);

    char *result = str_concat(parent_str, "\n");
    result = str_concat(result, "Variable ");
    result = str_concat(result, var_str);
    result = str_concat(result, " : ");
    result = str_concat(result, type_str);
    result = str_concat(result, ".");

    free(var_str);
    free(type_str);
    free(parent_str);

    return result;
}

char *_get_str_addr(Expression *expression) {
    char buf[(2 * sizeof(void *)) + 1];
    snprintf(buf, sizeof(buf), "%p", (void *)expression);
    return strdup(str_concat("var", buf));
}

char *_top_level_stringify_to_address(Expression *expression) {
    char *result = NULL;

    switch (expression->type) {
        case VAR_EXPRESSION: {
            char buf[(2 * sizeof(void *)) + 1];
            snprintf(buf, sizeof(buf), "%p", (void *)expression);
            result = strdup(str_concat("var", buf));
            break;
        }

        case APP_EXPRESSION: {
            char buf1[(2 * sizeof(void *)) + 1];
            snprintf(buf1, sizeof(buf1), "%p",
                     (void *)expression->value.app.func);
            result = strdup(str_concat("var", buf1));

            char buf2[(2 * sizeof(void *)) + 1];
            snprintf(buf2, sizeof(buf2), "%p",
                     (void *)expression->value.app.arg);
            result = strdup(
                str_concat(str_concat(result, " "), str_concat("var", buf2)));
            result = parenthesize_and_free(result);
            break;
        }

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

void _count_subexpression(Expression *expression, Map *visited, Map *counts) {
    if (map_get(visited, expression)) {
        return;
    }
    bool *have_visited = malloc(sizeof(bool));

    *have_visited = true;  // truly, this can be any non-zero junk. TODO:

    // make this a set

    map_set(visited, expression, have_visited);

    switch (expression->type) {
        case VAR_EXPRESSION:
            break;
        case APP_EXPRESSION: {
            int *func_lookup_value =
                map_get(counts, expression->value.app.func);
            if (!func_lookup_value) {
                int *value = malloc(sizeof(int));
                *value = 1;
                map_set(counts, (void *)expression->value.app.func, value);
            } else {
                (*func_lookup_value)++;
            }

            int *arg_lookup_value = map_get(counts, expression->value.app.arg);
            if (!arg_lookup_value) {
                int *value = malloc(sizeof(int));
                *value = 1;
                map_set(counts, (void *)expression->value.app.arg, value);
            } else {
                (*arg_lookup_value)++;
            }

            _count_subexpression(expression->value.app.func, visited, counts);
            _count_subexpression(expression->value.app.arg, visited, counts);
            break;
        }

        default:
            break;
    }
}

DoublyLinkedList *topo_order(Expression *top_expr, Map *expr_counts) {
    // We assume top_expr is NOT in expr_counts, since it should always have
    // in-degree 0.

    DoublyLinkedList *L =
        dll_create();  // Empty list that will contain the sorted elements
    DoublyLinkedList *S =
        dll_create();  // Set of all nodes with no incoming edge
    dll_insert_at_head(S, dll_new_node(top_expr));

    while (dll_len(S) != 0) {
        DLLNode *n = dll_remove_head(S);
        dll_insert_at_tail(L, n);

        Expression *expr = (Expression *)n->data;

        // Reduce counts for all of expr's immediate children
        switch (expr->type) {
            case (VAR_EXPRESSION):
                break;
            case (APP_EXPRESSION): {
                Expression *func = expr->value.app.func;
                int *func_count = map_get(expr_counts, func);
                (*func_count)--;

                Expression *arg = expr->value.app.arg;
                int *arg_count = map_get(expr_counts, arg);
                (*arg_count)--;
                break;
            }
            default:
                break;
        }

        // For each of expr's immediate children who have a count of 0, add them
        // to S.
        switch (expr->type) {
            case (VAR_EXPRESSION):
                break;
            case (APP_EXPRESSION): {
                Expression *func = expr->value.app.func;
                int *func_count = map_get(expr_counts, func);
                if (*func_count == 0) {
                    dll_insert_at_tail(S, dll_new_node(func));
                }
                Expression *arg = expr->value.app.arg;
                int *arg_count = map_get(expr_counts, arg);
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

char *sc(Context *context) { return stringify_context(context); }

char *_stringify_expression_with_let(Expression *expression) {
    char *result = NULL;

    switch (expression->type) {
        case VAR_EXPRESSION:
            result = strdup(expression->value.var.name);
            break;

        case LAMBDA_EXPRESSION: {
            if (dll_len(get_expression_uplinks(expression)) > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *var_str = _stringify_expression_with_let(

                expression->value.lambda.bound_variable);

            char *type_str = _stringify_expression_with_let(

                expression->value.lambda.bound_variable->value.var.type);

            char *body_str = _stringify_expression_with_let(

                expression->value.lambda.body);

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
            if (dll_len(get_expression_uplinks(expression)) > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *func_str =

                _stringify_expression_with_let(expression->value.app.func);

            char *arg_str =

                _stringify_expression_with_let(expression->value.app.arg);

            char *app_str = str_concat(func_str, " ");

            app_str = str_concat(app_str, arg_str);

            result = parenthesize_and_free(app_str);

            free(func_str);

            free(arg_str);

            break;
        }

        case FORALL_EXPRESSION: {
            if (dll_len(get_expression_uplinks(expression)) > 1) {
                char buf[(2 * sizeof(void *)) + 1];
                snprintf(buf, sizeof(buf), "%p", (void *)expression);
                result = strdup(str_concat("var", buf));
                break;
            }
            char *var_str = _stringify_expression_with_let(

                expression->value.forall.bound_variable);

            char *type_str = _stringify_expression_with_let(

                expression->value.forall.bound_variable->value.var.type);

            char *body_str = _stringify_expression_with_let(

                expression->value.forall.body);

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
            result = str_concat("?", strdup(expression->value.hole.name));
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

char *_top_level_stringify_expression_with_let(Expression *expression) {
    char *result = NULL;

    switch (expression->type) {
        case VAR_EXPRESSION:
            result = strdup(expression->value.var.name);
            break;

        case LAMBDA_EXPRESSION: {
            char *var_str = _stringify_expression_with_let(
                expression->value.lambda.bound_variable);
            char *type_str = _stringify_expression_with_let(
                expression->value.lambda.bound_variable->value.var.type);
            char *body_str =
                _stringify_expression_with_let(expression->value.lambda.body);
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
            char *func_str =
                _stringify_expression_with_let(expression->value.app.func);
            char *arg_str =
                _stringify_expression_with_let(expression->value.app.arg);

            char *app_str = str_concat(func_str, " ");
            app_str = str_concat(app_str, arg_str);

            result = parenthesize_and_free(app_str);
            free(func_str);
            free(arg_str);
            break;
        }

        case FORALL_EXPRESSION: {
            char *var_str = _stringify_expression_with_let(
                expression->value.forall.bound_variable);
            char *type_str = _stringify_expression_with_let(
                expression->value.forall.bound_variable->value.var.type);
            char *body_str =
                _stringify_expression_with_let(expression->value.forall.body);
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
            result = str_concat("?", strdup(expression->value.hole.name));
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

char *stringify_expression_with_let(Expression *expression) {
    Map *visited = map_new();  // maps addresses to visited bool
    Map *counts = map_new();   // maps addresses to counts
    _count_subexpression(expression, visited, counts);
    DoublyLinkedList *ordering = topo_order(expression, counts);

    char *result = NULL;
    for (int i = dll_len(ordering) - 1; i >= 0; i--) {
        DLLNode *node = dll_at(ordering, i);
        Expression *node_expr = (Expression *)node->data;

        if (node_expr->type != VAR_EXPRESSION &&
            dll_len(get_expression_uplinks(node_expr)) > 1) {
            char *str_adr = _get_str_addr(node_expr);
            char *expr_string =
                _top_level_stringify_expression_with_let(node_expr);
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
    char *final_output = (result == NULL)
                             ? stringified_expr
                             : str_concat(result, stringified_expr);
    return final_output;
}