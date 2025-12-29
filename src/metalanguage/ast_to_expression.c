#include "ast_to_expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/lexer.h"
#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/expression.h"
#include "src/metalanguage/parser.h"

/**
 * Simple linked list node for storing let bindings with string keys.
 */
typedef struct LetBinding {
    char *name;
    Expression *value;
} LetBinding;

/**
 * Internal helper for recursive conversion.
 * Maintains a working context that includes newly bound variables.
 *
 * @param ast The AST node to convert.
 * @param context The current context including any bound variables.
 * @param letbindings A linked list of LetBinding structs for variable
 * substitutions.
 * @return The converted Expression, or NULL on failure.
 */
static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings);

/**
 * Helper to look up a let binding by name.
 */
static Expression *letbindings_get(DoublyLinkedList *letbindings, const char *name) {
    if (!letbindings || !name) {
        return NULL;
    }

    DLLNode *current = letbindings->head;
    while (current) {
        LetBinding *binding = (LetBinding *)current->data;
        if (binding && strcmp(binding->name, name) == 0) {
            return binding->value;
        }
        current = current->next;
    }
    return NULL;
}

/**
 * Helper to add a let binding.
 */
static void letbindings_set(DoublyLinkedList *letbindings, const char *name, Expression *value) {
    if (!letbindings || !name) {
        return;
    }

    LetBinding *binding = (LetBinding *)malloc(sizeof(LetBinding));
    if (!binding) {
        return;
    }

    binding->name = strdup(name);
    binding->value = value;
    dll_insert_at_tail(letbindings, dll_new_node(binding));
}

static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings) {
    if (!ast) {
        return NULL;
    }

    switch (ast->tag) {
        case AST_VAR: {
            Expression *letbinding = letbindings_get(letbindings, ast->value.var.name);
            if (letbinding) {
                return letbinding;
            }

            return context_lookup_by_name(context, ast->value.var.name);
        }

        case AST_TYPE:
            return init_type_expression();

        case AST_PROP:
            return init_prop_expression();

        case AST_LAMBDA: {
            Expression *binder_type =
                _ast_to_expression(ast->value.lambda.binder.type, context, letbindings);
            if (!binder_type) {
                return NULL;
            }

            char *name = ast->value.lambda.binder.name;

            Expression *bound_var = init_var_expression_wc(name, binder_type, context);
            if (!bound_var) {
                return NULL;
            }

            Context *extended_context = bound_var;
            if (!extended_context) {
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.lambda.body, extended_context, letbindings);
            if (!body) {
                return NULL;
            }

            return init_lambda_expression_wc(bound_var, body);
        }

        case AST_FORALL: {
            Expression *binder_type =
                _ast_to_expression(ast->value.forall.binder.type, context, letbindings);
            if (!binder_type) {
                return NULL;
            }

            char *name = ast->value.forall.binder.name;

            Expression *bound_var = init_var_expression_wc(name, binder_type, context);
            if (!bound_var) {
                return NULL;
            }

            Context *extended_context = bound_var;
            if (!extended_context) {
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.forall.body, extended_context, letbindings);
            if (!body) {
                return NULL;
            }

            return init_forall_expression_wc(bound_var, body);
        }

        case AST_APP: {
            Expression *func = _ast_to_expression(ast->value.app.func, context, letbindings);
            if (!func) {
                return NULL;
            }

            Expression *arg = _ast_to_expression(ast->value.app.arg, context, letbindings);
            if (!arg) {
                return NULL;
            }

            return init_app_expression_wc(func, arg, context);
        }

        case AST_LET: {
            Expression *type = _ast_to_expression(ast->value.let.type, context, letbindings);
            if (!type) {
                return NULL;
            }

            Expression *value = _ast_to_expression(ast->value.let.value, context, letbindings);
            if (!value) {
                return NULL;
            }

            // Note: something to consider is the fact that we could using an
            // identifier that is already in use in the context. wrt the kernel,
            // this doesn't matter. But for the user, this could be confusing so
            // we'll fail here.
            if (context_contains_name(context, ast->value.let.name)) {
                return NULL;
            }

            letbindings_set(letbindings, ast->value.let.name, value);

            return _ast_to_expression(ast->value.let.body, context, letbindings);
        }

        case AST_MATCH: {
            Expression *scrutinee =
                _ast_to_expression(ast->value.match.scrutinee, context, letbindings);
            if (!scrutinee) {
                return NULL;
            }

            int branch_count = ast->value.match.branch_count;
            MatchBranch **branches = malloc(branch_count * sizeof(MatchBranch *));
            if (!branches) {
                return NULL;
            }

            for (int i = 0; i < branch_count; i++) {
                AST *branch_ast = ast->value.match.branches[i];
                Pattern *pattern = branch_ast->value.matchbranch.pattern;

                MatchBranch *branch = malloc(sizeof(MatchBranch));
                if (!branch) {
                    // TODO: cleanup
                    return NULL;
                }

                Expression *constructor =
                    context_lookup_by_name(context, pattern->constructor_name);
                if (!constructor) {
                    fprintf(stderr, ERROR "Constructor %s not found in context.\n" CRESET,
                            pattern->constructor_name);
                    free(branch);
                    return NULL;
                }
                branch->constructor = constructor;

                // Create pattern variables
                branch->pattern_var_count = pattern->argument_count;
                branch->pattern_variables = NULL;

                if (branch->pattern_var_count > 0) {
                    branch->pattern_variables =
                        malloc(branch->pattern_var_count * sizeof(Expression *));

                    Expression *ctor_type = get_expression_type(constructor);
                    Context *extended_context = context;

                    for (int j = 0; j < branch->pattern_var_count; j++) {
                        if (ctor_type->tag != FORALL_EXPRESSION) {
                            fprintf(stderr, ERROR
                                    "Constructor has fewer parameters than pattern.\n" CRESET);
                            free(branch->pattern_variables);
                            free(branch);
                            return NULL;
                        }

                        Expression *bound_var = get_forall_bound_variable(ctor_type);
                        Expression *param_type = get_expression_type(bound_var);

                        Expression *pattern_var = init_var_expression_wc(
                            pattern->argument_names[j], param_type, extended_context);

                        branch->pattern_variables[j] = pattern_var;

                        extended_context = pattern_var;

                        ctor_type = get_forall_body(ctor_type);
                    }

                    branch->body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                      extended_context, letbindings);
                } else {
                    branch->body = _ast_to_expression(branch_ast->value.matchbranch.body, context,
                                                      letbindings);
                }

                if (!branch->body) {
                    free(branch->pattern_variables);
                    free(branch);
                    return NULL;
                }

                branches[i] = branch;
            }

            Expression *match_expr =
                init_match_expression_wc(scrutinee, branches, branch_count, context);
            if (!match_expr) {
                for (int i = 0; i < branch_count; i++) {
                    if (branches[i]) {
                        free(branches[i]->pattern_variables);
                        free(branches[i]);
                    }
                }
                free(branches);
                return NULL;
            }

            return match_expr;
        }
        default:
            return NULL;
    }
}

Expression *ast_to_expression(AST *ast, Context *context) {
    if (!ast || !context) {
        return NULL;
    }

    DoublyLinkedList *letbindings = dll_create();
    Expression *result = _ast_to_expression(ast, context, letbindings);

    // Free the letbindings list and its contents
    if (letbindings) {
        DLLNode *current = letbindings->head;
        while (current) {
            LetBinding *binding = (LetBinding *)current->data;
            if (binding) {
                free(binding->name);
                free(binding);
            }
            current = current->next;
        }
        dll_destroy(letbindings);
    }

    return result;
}

Expression *parse_string_to_expression(const char *input, Context *context) {
    if (!input || !context) {
        return NULL;
    }

    // todo: refactor to argument
    MEngineOptions options = {.debug = false};

    Lexer lexer;
    lexer_init(&lexer, input, &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

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
            break;

        case AST_MATCHBRANCH:
            if (ast->value.matchbranch.pattern) {
                free(ast->value.matchbranch.pattern->constructor_name);
                for (int i = 0; i < ast->value.matchbranch.pattern->argument_count; i++) {
                    free(ast->value.matchbranch.pattern->argument_names[i]);
                }
                free(ast->value.matchbranch.pattern->argument_names);
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