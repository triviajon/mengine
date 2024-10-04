#include "utils.h"

// Helper function to concatenate two strings
char *str_concat(const char *s1, const char *s2) {
    char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
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

    switch (expression->type) {
        case VAR_EXPRESSION:
            result = strdup(expression->value.var.name);
            break;

        case LAMBDA_EXPRESSION: {
            char *ctx_str = stringify_context(expression->value.lambda.context);
            char *body_str = stringify_expression(expression->value.lambda.body);
            result = str_concat("fun ", ctx_str);
            result = str_concat(result, " -> ");
            result = str_concat(result, body_str);
            free(ctx_str);
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
            char *ctx_str = stringify_context(expression->value.forall.context); 
            char *body_str = stringify_expression(expression->value.forall.body);
            result = str_concat("∀ ", ctx_str);
            result = str_concat(result, " : ");
            result = str_concat(result, body_str);
            free(ctx_str);
            free(body_str);
            break;
        }

        case TYPE_EXPRESSION:
            result = strdup("Type");
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}


char *stringify_context(Context *context) {
    if (context_is_empty(context)) {
        return strdup("");
    } else {
        char *var_str = stringify_expression(context->variable);
        char *type_str = stringify_expression(context->type);
        char *parent_str = stringify_context(context->parent);

        char *result = str_concat("[", var_str);
        result = str_concat(result, " : ");
        result = str_concat(result, type_str);
        result = str_concat(result, "]");
        result = str_concat(result, parent_str);

        free(var_str);
        free(type_str);
        free(parent_str);

        return result;
    }
}

bool expression_equal(Expression *a, Expression *b) {
  return a == b;
}