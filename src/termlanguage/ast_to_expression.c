#include "ast_to_expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/common/lexer.h"
#include "src/engine/tactic_api.h"
#include "src/kernel/kernel_api.h"
#include "src/termlanguage/parser.h"

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

            Expression *val = kernel_context_lookup(context, ast->value.var.name);
            if (!val) {
                fprintf(stderr, ERROR "Variable %s not found in context.\n" CRESET,
                        ast->value.var.name);
                return NULL;
            }
            return val;
        }

        case AST_TYPE:
            return kernel_type_create();

        case AST_PROP:
            return kernel_prop_create();

        case AST_EXPR_REF:
            return tactic_value_as_expr(ast->value.expr_ref.tval);

        case AST_LAMBDA: {
            Expression *binder_type =
                _ast_to_expression(ast->value.lambda.binder.type, context, letbindings);
            if (!binder_type) {
                fprintf(stderr, ERROR "Failed to create lambda binder type.\n" CRESET);
                return NULL;
            }

            char *name = ast->value.lambda.binder.name;

            Expression *bound_var = kernel_var_create(name, binder_type, context);
            if (!bound_var) {
                fprintf(stderr, ERROR "Failed to create lambda bound var.\n" CRESET);
                return NULL;
            }

            Context *extended_context = bound_var;
            if (!extended_context) {
                fprintf(stderr, ERROR "Failed to create lambda extended context.\n" CRESET);
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.lambda.body, extended_context, letbindings);
            if (!body) {
                fprintf(stderr, ERROR "Failed to create lambda body.\n" CRESET);
                return NULL;
            }

            return kernel_lambda_create(bound_var, body);
        }

        case AST_FORALL: {
            Expression *binder_type =
                _ast_to_expression(ast->value.forall.binder.type, context, letbindings);
            if (!binder_type) {
                fprintf(stderr, ERROR "Failed to create forall binder type.\n" CRESET);
                return NULL;
            }

            char *name = ast->value.forall.binder.name;

            Expression *bound_var = kernel_var_create(name, binder_type, context);
            if (!bound_var) {
                fprintf(stderr, ERROR "Failed to create forall bound var.\n" CRESET);
                return NULL;
            }

            Context *extended_context = bound_var;
            if (!extended_context) {
                fprintf(stderr, ERROR "Failed to create forall extended context.\n" CRESET);
                return NULL;
            }

            Expression *body =
                _ast_to_expression(ast->value.forall.body, extended_context, letbindings);
            if (!body) {
                fprintf(stderr, ERROR "Failed to create forall body.\n" CRESET);
                return NULL;
            }

            return kernel_forall_create(bound_var, body);
        }

        case AST_APP: {
            Expression *func = _ast_to_expression(ast->value.app.func, context, letbindings);
            if (!func) {
                fprintf(stderr, ERROR "Failed to create app func.\n" CRESET);
                return NULL;
            }

            Expression *arg = _ast_to_expression(ast->value.app.arg, context, letbindings);
            if (!arg) {
                fprintf(stderr, ERROR "Failed to create app arg.\n" CRESET);
                return NULL;
            }

            return kernel_app_create(func, arg, context);
        }

        case AST_LET: {
            Expression *type = _ast_to_expression(ast->value.let.type, context, letbindings);
            if (!type) {
                fprintf(stderr, ERROR "Failed to create let type.\n" CRESET);
                return NULL;
            }

            Expression *value = _ast_to_expression(ast->value.let.value, context, letbindings);
            if (!value) {
                fprintf(stderr, ERROR "Failed to create let value.\n" CRESET);
                return NULL;
            }

            // Note: something to consider is the fact that we could using an
            // identifier that is already in use in the context. wrt the kernel,
            // this doesn't matter. But for the user, this could be confusing so
            // we'll fail here.
            if (kernel_context_has_name(context, ast->value.let.name)) {
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
            void **branches = malloc(branch_count * sizeof(void *));
            if (!branches) {
                return NULL;
            }

            for (int i = 0; i < branch_count; i++) {
                AST *branch_ast = ast->value.match.branches[i];
                Pattern *pattern = branch_ast->value.matchbranch.pattern;

                Expression *constructor = kernel_context_lookup(context, pattern->constructor_name);
                if (!constructor) {
                    fprintf(stderr, ERROR "Constructor %s not found in context.\n" CRESET,
                            pattern->constructor_name);
                    free(branches);
                    return NULL;
                }

                // Create pattern variables
                int pattern_var_count = pattern->argument_count;
                Expression **pattern_variables = NULL;

                if (pattern_var_count > 0) {
                    pattern_variables = malloc(pattern_var_count * sizeof(Expression *));

                    Expression *ctor_type = kernel_expr_type(constructor);
                    Context *extended_context = context;

                    for (int j = 0; j < pattern_var_count; j++) {
                        Expression *bound_var = kernel_forall_var(ctor_type);
                        if (!bound_var) {
                            fprintf(stderr, ERROR
                                    "Constructor has fewer parameters than pattern.\n" CRESET);
                            free(pattern_variables);
                            free(branches);
                            return NULL;
                        }

                        Expression *param_type = kernel_expr_type(bound_var);

                        Expression *pattern_var = kernel_var_create(pattern->argument_names[j],
                                                                    param_type, extended_context);

                        pattern_variables[j] = pattern_var;

                        extended_context = pattern_var;

                        ctor_type = kernel_forall_body(ctor_type);
                    }

                    Expression *body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                          extended_context, letbindings);
                    if (!body) {
                        fprintf(stderr, ERROR "Failed to create match branch body.\n" CRESET);
                        free(pattern_variables);
                        free(branches);
                        return NULL;
                    }

                    branches[i] = kernel_match_branch_create(constructor, pattern_variables,
                                                             pattern_var_count, body);
                } else {
                    Expression *body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                          context, letbindings);
                    if (!body) {
                        fprintf(stderr, ERROR "Failed to create match branch body.\n" CRESET);
                        free(branches);
                        return NULL;
                    }

                    branches[i] =
                        kernel_match_branch_create(constructor, /*pattern_variables*/ NULL,
                                                   /*pattern_var_count*/ 0, body);
                }

                if (!branches[i]) {
                    fprintf(stderr, ERROR "Failed to create match branch.\n" CRESET);
                    free(pattern_variables);
                    free(branches);
                    return NULL;
                }
            }

            Expression *match_expr =
                kernel_match_create(scrutinee, branches, branch_count, context);
            if (!match_expr) {
                fprintf(stderr, ERROR "Failed to create match expression.\n" CRESET);
                for (int i = 0; i < branch_count; i++) {
                    kernel_match_branch_free(branches[i]);
                }
                free(branches);
                return NULL;
            }

            return match_expr;
        }

        case AST_FIX: {
            int binder_count = ast->value.fix.binder_count;
            Expression *recursive_var = NULL;
            {
                Context **contexts = malloc((binder_count + 1) * sizeof(Context *));
                if (!contexts) {
                    fprintf(stderr, ERROR "Memory allocation failed for contexts.\n" CRESET);
                    return NULL;
                }

                contexts[0] = context;
                for (int i = 0; i < binder_count; i++) {
                    Expression *binder_type = _ast_to_expression(ast->value.fix.binders[i]->type,
                                                                 contexts[i], letbindings);
                    if (!binder_type) {
                        fprintf(stderr,
                                ERROR "Failed to convert binder type in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    Expression *binder = kernel_var_create(ast->value.fix.binders[i]->name,
                                                           binder_type, contexts[i]);
                    if (!binder) {
                        fprintf(stderr,
                                ERROR "Failed to create binder in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    contexts[i + 1] = binder;
                }

                Expression *return_type = _ast_to_expression(ast->value.fix.return_type,
                                                             contexts[binder_count], letbindings);
                if (!return_type) {
                    fprintf(stderr,
                            ERROR "Failed to convert return type in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }

                // recursive var type: forall (x1 : A1) ... (xn : An), return_type
                Expression *rec_var_type = return_type;
                for (int i = binder_count - 1; i >= 0; i--) {
                    rec_var_type = kernel_forall_create(contexts[i + 1], rec_var_type);
                    if (!rec_var_type) {
                        fprintf(stderr, ERROR "Failed to create forall type.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }
                }

                recursive_var = kernel_var_create(ast->value.fix.name, rec_var_type, context);
                if (!recursive_var) {
                    fprintf(stderr,
                            ERROR "Failed to create recursive var in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }
                free(contexts);
            }
            // Next, make the fix expression

            Expression *fix_expr = NULL;
            {
                Context **contexts = malloc((binder_count + 1) * sizeof(Context *));
                if (!contexts) {
                    fprintf(stderr, ERROR "Memory allocation failed for contexts.\n" CRESET);
                    return NULL;
                }

                contexts[0] = recursive_var;
                for (int i = 0; i < binder_count; i++) {
                    Expression *binder_type = _ast_to_expression(ast->value.fix.binders[i]->type,
                                                                 contexts[i], letbindings);
                    if (!binder_type) {
                        fprintf(stderr,
                                ERROR "Failed to convert binder type in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    Expression *binder = kernel_var_create(ast->value.fix.binders[i]->name,
                                                           binder_type, contexts[i]);
                    if (!binder) {
                        fprintf(stderr,
                                ERROR "Failed to create binder in fix expression.\n" CRESET);
                        free(contexts);
                        return NULL;
                    }

                    contexts[i + 1] = binder;
                }

                Expression *body =
                    _ast_to_expression(ast->value.fix.body, contexts[binder_count], letbindings);
                if (!body) {
                    fprintf(stderr, ERROR "Failed to convert body in fix expression.\n" CRESET);
                    free(contexts);
                    return NULL;
                }

                Expression *decreasing_arg = kernel_context_lookup(
                    contexts[binder_count], ast->value.fix.decreasing_arg_name);
                if (!decreasing_arg) {
                    fprintf(stderr, ERROR "Decreasing argument '%s' not found in context.\n" CRESET,
                            ast->value.fix.decreasing_arg_name);
                    free(contexts);
                    return NULL;
                }

                int decreasing_arg_index = -1;
                for (int i = 0; i < binder_count; i++) {
                    if (decreasing_arg == contexts[i + 1]) {
                        decreasing_arg_index = i;
                        break;
                    }
                }

                if (decreasing_arg_index == -1) {
                    fprintf(stderr, ERROR "Decreasing argument '%s' not found in binders.\n" CRESET,
                            ast->value.fix.decreasing_arg_name);
                    free(contexts);
                    return NULL;
                }

                fix_expr = kernel_fix_create(recursive_var, contexts + 1, binder_count, body,
                                             decreasing_arg_index, contexts[0]);
                free(contexts);
                if (!fix_expr) {
                    fprintf(stderr, ERROR "Failed to create fix expression.\n" CRESET);
                    return NULL;
                }
            }

            // return the recursive variable here-- the fix expression will be registered as the
            // body of the recursive variable
            return recursive_var;
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
                for (size_t i = 0; i < ast->value.match.branch_count; i++) {
                    free_ast(ast->value.match.branches[i]);
                }
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

        case AST_LET:
            free(ast->value.let.name);
            free_ast(ast->value.let.type);
            free_ast(ast->value.let.value);
            free_ast(ast->value.let.body);
            break;

        case AST_FIX:
            free(ast->value.fix.name);
            if (ast->value.fix.binders) {
                for (size_t i = 0; i < ast->value.fix.binder_count; i++) {
                    if (ast->value.fix.binders[i]) {
                        free(ast->value.fix.binders[i]->name);
                        free_ast(ast->value.fix.binders[i]->type);
                        free(ast->value.fix.binders[i]);
                    }
                }
                free(ast->value.fix.binders);
            }
            free(ast->value.fix.decreasing_arg_name);
            free_ast(ast->value.fix.return_type);
            free_ast(ast->value.fix.body);
            break;

        case AST_PATVAR:
            free(ast->value.patvar.name);
            break;

        case AST_EXPR_REF:
            free_tactic_value(ast->value.expr_ref.tval);
            break;

        case AST_TYPE:
        case AST_PROP:
        default:
            break;
    }

    free(ast);
}