#include "ast_to_expression.h"

#include <stdlib.h>
#include <string.h>

/**
 * Internal helper for recursive conversion.
 * Maintains a working context that includes newly bound variables.
 *
 * @param ast The AST node to convert.
 * @param context The current context including any bound variables.
 * @return The converted Expression, or NULL on failure.
 */
static Expression *_ast_to_expression(AST *ast, Context *context);

static Expression *_ast_to_expression(AST *ast, Context *context) {
    if (!ast) {
        return NULL;
    }

    switch (ast->tag) {
        case AST_VAR: {
            return context_lookup_by_name(context, ast->value.var.name);
        }

        case AST_TYPE:
            return init_type_expression();

        case AST_PROP:
            return init_prop_expression();

        case AST_LAMBDA: {
            Expression *binder_type =
                _ast_to_expression(ast->value.lambda.binder.type, context);
            if (!binder_type) {
                return NULL;
            }

            char *name = ast->value.lambda.binder.name;

            Expression *bound_var =
                init_var_expression_wc(name, binder_type, context);
            if (!bound_var) {
                return NULL;
            }

            // Handle "_" as anonymous binder
            if (name && strcmp(name, "_") == 0) {
                Expression *body =
                    _ast_to_expression(ast->value.lambda.body, context);
                if (!body) {
                    return NULL;
                }
                return init_lambda_expression_wc(bound_var, body, context);
            }

            Context *extended_context = context_insert(context, bound_var);
            if (!extended_context) {
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.lambda.body, extended_context);
            if (!body) {
                return NULL;
            }

            return init_lambda_expression_wc(bound_var, body, extended_context);
        }

        case AST_FORALL: {
            Expression *binder_type =
                _ast_to_expression(ast->value.forall.binder.type, context);
            if (!binder_type) {
                return NULL;
            }

            char *name = ast->value.forall.binder.name;

            Expression *bound_var =
                init_var_expression_wc(name, binder_type, context);
            if (!bound_var) {
                return NULL;
            }

            // Handle "_" as anonymous binder
            if (name && strcmp(name, "_") == 0) {
                Expression *body =
                    _ast_to_expression(ast->value.forall.body, context);
                if (!body) {
                    return NULL;
                }
                return init_forall_expression_wc(bound_var, body, context);
            }

            Context *extended_context = context_insert(context, bound_var);
            if (!extended_context) {
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.forall.body, extended_context);
            if (!body) {
                return NULL;
            }

            return init_forall_expression_wc(bound_var, body, extended_context);
        }

        case AST_APP: {
            Expression *func = _ast_to_expression(ast->value.app.func, context);
            if (!func) {
                return NULL;
            }

            Expression *arg = _ast_to_expression(ast->value.app.arg, context);
            if (!arg) {
                return NULL;
            }

            return init_app_expression_wc(func, arg, context);
        }

        case AST_MATCH: {
            // TODO: Implement match expression conversion
            return NULL;
        }

        case AST_MATCHBRANCH: {
            // TODO: Implement match branch conversion
            return NULL;
        }

        default:
            return NULL;
    }
}

Expression *ast_to_expression(AST *ast, Context *context) {
    if (!ast || !context) {
        return NULL;
    }

    return _ast_to_expression(ast, context);
}

Expression *parse_string_to_expression(const char *input, Context *context) {
    if (!input || !context) {
        return NULL;
    }

    Lexer lexer;
    lexer_init(&lexer, input);

    Parser parser;
    parser_init(&parser, &lexer);

    AST *ast = parse_term(&parser);
    if (!ast) {
        return NULL;
    }

    Expression *expr = ast_to_expression(ast, context);
    free_ast(ast);

    return expr;
}

void free_ast(AST *ast) {
    if (!ast) {
        return;
    }

    switch (ast->tag) {
        case AST_VAR:
            free(ast->value.var.name);
            break;

        case AST_LAMBDA:
            free(ast->value.lambda.binder.name);
            free_ast(ast->value.lambda.binder.type);
            free_ast(ast->value.lambda.body);
            break;

        case AST_FORALL:
            free(ast->value.forall.binder.name);
            free_ast(ast->value.forall.binder.type);
            free_ast(ast->value.forall.body);
            break;

        case AST_APP:
            free_ast(ast->value.app.func);
            free_ast(ast->value.app.arg);
            break;

        case AST_MATCH:
            free_ast(ast->value.match.scrutinee);
            if (ast->value.match.branches) {
                free(ast->value.match.branches);
            }
            if (ast->value.match.branch_count) {
                free(ast->value.match.branch_count);
            }
            break;

        case AST_MATCHBRANCH:
            if (ast->value.matchbranch.pattern) {
                free(ast->value.matchbranch.pattern->name);
                free(ast->value.matchbranch.pattern);
            }
            free_ast(ast->value.matchbranch.body);
            break;

        case AST_TYPE:
        case AST_PROP:
        default:
            break;
    }

    free(ast);
}