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

typedef struct BoundName {
    const char *name;
    struct BoundName *next;
} BoundName;

typedef struct SourceLetBinding {
    const char *name;
    Context *required_context;
    struct SourceLetBinding *next;
} SourceLetBinding;

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
static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings,
                                      TacticEnvEntry *tac_env);

static Context *ast_required_context(AST *ast, Context *lexical_context,
                                     DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                     BoundName *bound_names, SourceLetBinding *source_lets);

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

static bool bound_names_contains(BoundName *bound_names, const char *name) {
    for (BoundName *it = bound_names; it; it = it->next) {
        if (strcmp(it->name, name) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_anonymous_name(const char *name) {
    return name && name[0] == '_' && name[1] == '\0';
}

static BoundName *push_bound_name(BoundName *head, BoundName *node, const char *name) {
    if (is_anonymous_name(name)) {
        return head;
    }
    node->name = name;
    node->next = head;
    return node;
}

static Expression *dependency_context_lookup(Context *context, const char *name) {
    bool target_is_anonymous = is_anonymous_name(name);
    Context *current = context;
    while (current && current != kernel_context_empty()) {
        const char *current_name = kernel_var_name(current);
        if ((!target_is_anonymous && is_anonymous_name(current_name)) ||
            strcmp(current_name, name) != 0) {
            current = kernel_expr_context(current);
            continue;
        }
        return current;
    }
    return NULL;
}

static Context *source_lets_get(SourceLetBinding *source_lets, const char *name) {
    for (SourceLetBinding *it = source_lets; it; it = it->next) {
        if (strcmp(it->name, name) == 0) {
            return it->required_context;
        }
    }
    return NULL;
}

static Context *join_required_contexts(Context *lhs, Context *rhs) {
    if (!lhs || !rhs) return NULL;
    if (lhs == rhs) return lhs;
    if (kernel_context_is_ancestor(lhs, rhs)) return rhs;
    if (kernel_context_is_ancestor(rhs, lhs)) return lhs;
    fprintf(stderr, ERROR "Cannot join incomparable required contexts.\n" CRESET);
    return NULL;
}

static Context *require_context_valid(Context *required, Context *lexical_context,
                                      const char *source) {
    if (!required) {
        return NULL;
    }
    if (!kernel_context_is_ancestor(required, lexical_context)) {
        fprintf(stderr, ERROR "%s is not valid in the current lexical context.\n" CRESET,
                source);
        return NULL;
    }
    return required;
}

static Context *required_context_for_name(const char *name, Context *lexical_context,
                                          DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                          BoundName *bound_names,
                                          SourceLetBinding *source_lets) {
    Expression *letbinding = letbindings_get(letbindings, name);
    if (letbinding) {
        return require_context_valid(kernel_expr_context(letbinding), lexical_context,
                                     "Let-bound term");
    }

    Context *source_let_context = source_lets_get(source_lets, name);
    if (source_let_context) {
        return source_let_context;
    }

    for (TacticEnvEntry *e = tac_env; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            if (!e->val) {
                fprintf(stderr, ERROR "Tactic binding %s has no value.\n" CRESET, name);
                return NULL;
            }
            if (e->val->kind == TVAL_EXPRESSION) {
                return require_context_valid(kernel_expr_context(e->val->expr), lexical_context,
                                             "Tactic-bound term");
            }
            if (e->val->kind == TVAL_AST) {
                return ast_required_context(e->val->ast, lexical_context, letbindings, e->next,
                                            bound_names, source_lets);
            }
            fprintf(stderr, ERROR "Tactic binding %s is not a term.\n" CRESET, name);
            return NULL;
        }
    }

    if (bound_names_contains(bound_names, name)) {
        return kernel_context_empty();
    }

    Expression *var = dependency_context_lookup(lexical_context, name);
    if (!var) {
        fprintf(stderr, ERROR "Variable %s not found in context.\n" CRESET, name);
        return NULL;
    }
    return var;
}

static Context *required_context_for_branch(AST *branch_ast, Context *lexical_context,
                                            DoublyLinkedList *letbindings,
                                            TacticEnvEntry *tac_env, BoundName *bound_names,
                                            SourceLetBinding *source_lets) {
    if (!branch_ast || branch_ast->tag != AST_MATCHBRANCH) {
        fprintf(stderr, ERROR "Expected match branch in dependency analysis.\n" CRESET);
        return NULL;
    }

    Pattern *pattern = branch_ast->value.matchbranch.pattern;
    Expression *constructor = dependency_context_lookup(lexical_context, pattern->constructor_name);
    if (!constructor) {
        fprintf(stderr, ERROR "Constructor %s not found in context.\n" CRESET,
                pattern->constructor_name);
        return NULL;
    }

    BoundName *pattern_bound_names = bound_names;
    BoundName *pattern_nodes = NULL;
    if (pattern->argument_count > 0) {
        pattern_nodes = calloc((size_t)pattern->argument_count, sizeof(BoundName));
        if (!pattern_nodes) {
            fprintf(stderr, ERROR "Memory allocation failed during dependency analysis.\n" CRESET);
            return NULL;
        }
        for (int i = 0; i < pattern->argument_count; i++) {
            pattern_bound_names =
                push_bound_name(pattern_bound_names, &pattern_nodes[i],
                                pattern->argument_names[i]);
        }
    }

    Context *body_ctx =
        ast_required_context(branch_ast->value.matchbranch.body, lexical_context, letbindings,
                             tac_env, pattern_bound_names, source_lets);
    free(pattern_nodes);
    return join_required_contexts(constructor, body_ctx);
}

static Context *ast_required_context(AST *ast, Context *lexical_context,
                                     DoublyLinkedList *letbindings, TacticEnvEntry *tac_env,
                                     BoundName *bound_names, SourceLetBinding *source_lets) {
    if (!ast) {
        return kernel_context_empty();
    }

    switch (ast->tag) {
        case AST_VAR:
            return required_context_for_name(ast->value.var.name, lexical_context, letbindings,
                                             tac_env, bound_names, source_lets);
        case AST_TYPE:
        case AST_PROP:
            return kernel_context_empty();
        case AST_PATVAR:
            fprintf(stderr, ERROR "Pattern variable %s is not a term.\n" CRESET,
                    ast->value.patvar.name);
            return NULL;
        case AST_EXPR_REF: {
            Expression *expr = tactic_value_as_expr(ast->value.expr_ref.tval);
            if (!expr) {
                fprintf(stderr, ERROR "Expression reference does not contain a term.\n" CRESET);
                return NULL;
            }
            return require_context_valid(kernel_expr_context(expr), lexical_context,
                                         "Referenced term");
        }
        case AST_APP: {
            Context *func_ctx = ast_required_context(ast->value.app.func, lexical_context,
                                                    letbindings, tac_env, bound_names,
                                                    source_lets);
            Context *arg_ctx = ast_required_context(ast->value.app.arg, lexical_context,
                                                   letbindings, tac_env, bound_names,
                                                   source_lets);
            return join_required_contexts(func_ctx, arg_ctx);
        }
        case AST_LAMBDA: {
            Context *type_ctx =
                ast_required_context(ast->value.lambda.binder.type, lexical_context, letbindings,
                                     tac_env, bound_names, source_lets);
            BoundName bound_node;
            BoundName *bound = push_bound_name(bound_names, &bound_node,
                                               ast->value.lambda.binder.name);
            Context *body_ctx = ast_required_context(ast->value.lambda.body, lexical_context,
                                                    letbindings, tac_env, bound, source_lets);
            return join_required_contexts(type_ctx, body_ctx);
        }
        case AST_FORALL: {
            Context *type_ctx =
                ast_required_context(ast->value.forall.binder.type, lexical_context, letbindings,
                                     tac_env, bound_names, source_lets);
            BoundName bound_node;
            BoundName *bound = push_bound_name(bound_names, &bound_node,
                                               ast->value.forall.binder.name);
            Context *body_ctx = ast_required_context(ast->value.forall.body, lexical_context,
                                                    letbindings, tac_env, bound, source_lets);
            return join_required_contexts(type_ctx, body_ctx);
        }
        case AST_LET: {
            Context *type_ctx = ast_required_context(ast->value.let.type, lexical_context,
                                                    letbindings, tac_env, bound_names,
                                                    source_lets);
            Context *value_ctx = ast_required_context(ast->value.let.value, lexical_context,
                                                     letbindings, tac_env, bound_names,
                                                     source_lets);
            Context *binding_ctx = join_required_contexts(type_ctx, value_ctx);
            if (!binding_ctx) {
                return NULL;
            }
            SourceLetBinding source_let = {ast->value.let.name, value_ctx, source_lets};
            Context *body_ctx = ast_required_context(ast->value.let.body, lexical_context,
                                                    letbindings, tac_env, bound_names,
                                                    &source_let);
            return join_required_contexts(binding_ctx, body_ctx);
        }
        case AST_MATCH: {
            Context *required = ast_required_context(ast->value.match.scrutinee, lexical_context,
                                                    letbindings, tac_env, bound_names,
                                                    source_lets);
            for (size_t i = 0; required && i < ast->value.match.branch_count; i++) {
                Context *branch_ctx =
                    required_context_for_branch(ast->value.match.branches[i], lexical_context,
                                                letbindings, tac_env, bound_names, source_lets);
                required = join_required_contexts(required, branch_ctx);
            }
            return required;
        }
        case AST_MATCHBRANCH:
            return required_context_for_branch(ast, lexical_context, letbindings, tac_env,
                                               bound_names, source_lets);
        case AST_FIX: {
            size_t binder_count = ast->value.fix.binder_count;
            BoundName *binder_nodes = NULL;
            if (binder_count > 0) {
                binder_nodes = calloc(binder_count, sizeof(BoundName));
                if (!binder_nodes) {
                    fprintf(stderr, ERROR
                            "Memory allocation failed during dependency analysis.\n" CRESET);
                    return NULL;
                }
            }

            Context *required = kernel_context_empty();
            BoundName *local_bound_names = bound_names;
            for (size_t i = 0; required && i < binder_count; i++) {
                Context *binder_type_ctx =
                    ast_required_context(ast->value.fix.binders[i]->type, lexical_context,
                                         letbindings, tac_env, local_bound_names, source_lets);
                required = join_required_contexts(required, binder_type_ctx);

                local_bound_names =
                    push_bound_name(local_bound_names, &binder_nodes[i],
                                    ast->value.fix.binders[i]->name);
            }

            Context *return_ctx = NULL;
            if (required) {
                return_ctx = ast_required_context(ast->value.fix.return_type, lexical_context,
                                                 letbindings, tac_env, local_bound_names,
                                                 source_lets);
                required = join_required_contexts(required, return_ctx);
            }

            if (required) {
                BoundName recursive_name_node;
                BoundName *recursive_name =
                    push_bound_name(local_bound_names, &recursive_name_node, ast->value.fix.name);
                Context *body_ctx = ast_required_context(ast->value.fix.body, lexical_context,
                                                        letbindings, tac_env, recursive_name,
                                                        source_lets);
                required = join_required_contexts(required, body_ctx);
            }

            if (required) {
                bool found_decreasing_arg = false;
                for (size_t i = 0; i < binder_count; i++) {
                    if (strcmp(ast->value.fix.binders[i]->name,
                               ast->value.fix.decreasing_arg_name) == 0) {
                        found_decreasing_arg = true;
                        break;
                    }
                }
                if (!found_decreasing_arg) {
                    fprintf(stderr, ERROR "Decreasing argument '%s' not found in binders.\n" CRESET,
                            ast->value.fix.decreasing_arg_name);
                    required = NULL;
                }
            }

            free(binder_nodes);
            return required;
        }
    }

    fprintf(stderr, ERROR "Unhandled AST tag in dependency analysis.\n" CRESET);
    return NULL;
}

static Context *binder_scope_context(Binder *binder, AST *body, Context *lexical_context,
                                     DoublyLinkedList *letbindings, TacticEnvEntry *tac_env) {
    Context *type_ctx = ast_required_context(binder->type, lexical_context, letbindings, tac_env,
                                             /*bound_names*/ NULL, /*source_lets*/ NULL);
    BoundName bound_node;
    BoundName *bound = push_bound_name(NULL, &bound_node, binder->name);
    Context *body_ctx =
        ast_required_context(body, lexical_context, letbindings, tac_env, bound,
                             /*source_lets*/ NULL);
    return join_required_contexts(type_ctx, body_ctx);
}

static Expression *_ast_to_expression(AST *ast, Context *context, DoublyLinkedList *letbindings,
                                      TacticEnvEntry *tac_env) {
    if (!ast) {
        return NULL;
    }

    switch (ast->tag) {
        case AST_VAR: {
            Expression *letbinding = letbindings_get(letbindings, ast->value.var.name);
            if (letbinding) {
                return letbinding;
            }

            // Check tactic let-binding environment (from TAC_LET / TAC_MATCH_*)
            for (TacticEnvEntry *e = tac_env; e; e = e->next) {
                if (strcmp(e->name, ast->value.var.name) == 0) {
                    if (!e->val) return NULL;
                    if (e->val->kind == TVAL_EXPRESSION) return e->val->expr;
                    if (e->val->kind == TVAL_AST) {
                        /* Evaluate in lexical parent env to avoid self-recursion. */
                        return _ast_to_expression(e->val->ast, context, letbindings, e->next);
                    }
                    /* TVAL_PAIR: cannot appear in term position */
                    return NULL;
                }
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
            Context *scope_context =
                binder_scope_context(&ast->value.lambda.binder, ast->value.lambda.body, context,
                                     letbindings, tac_env);
            if (!scope_context) {
                fprintf(stderr, ERROR "Failed to find lambda binder scope.\n" CRESET);
                return NULL;
            }
            Expression *binder_type =
                _ast_to_expression(ast->value.lambda.binder.type, scope_context, letbindings,
                                   tac_env);
            if (!binder_type) {
                fprintf(stderr, ERROR "Failed to create lambda binder type.\n" CRESET);
                return NULL;
            }

            char *name = ast->value.lambda.binder.name;

            Expression *bound_var = kernel_var_create(name, binder_type, scope_context);
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
                _ast_to_expression(ast->value.lambda.body, extended_context, letbindings, tac_env);
            if (!body) {
                fprintf(stderr, ERROR "Failed to create lambda body.\n" CRESET);
                return NULL;
            }

            return kernel_lambda_create(bound_var, body);
        }

        case AST_FORALL: {
            Context *scope_context =
                binder_scope_context(&ast->value.forall.binder, ast->value.forall.body, context,
                                     letbindings, tac_env);
            if (!scope_context) {
                fprintf(stderr, ERROR "Failed to find forall binder scope.\n" CRESET);
                return NULL;
            }
            Expression *binder_type =
                _ast_to_expression(ast->value.forall.binder.type, scope_context, letbindings,
                                   tac_env);
            if (!binder_type) {
                fprintf(stderr, ERROR "Failed to create forall binder type.\n" CRESET);
                return NULL;
            }

            char *name = ast->value.forall.binder.name;

            Expression *bound_var = kernel_var_create(name, binder_type, scope_context);
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
                _ast_to_expression(ast->value.forall.body, extended_context, letbindings, tac_env);
            if (!body) {
                fprintf(stderr, ERROR "Failed to create forall body.\n" CRESET);
                return NULL;
            }

            return kernel_forall_create(bound_var, body);
        }

        case AST_APP: {
            Expression *func =
                _ast_to_expression(ast->value.app.func, context, letbindings, tac_env);
            if (!func) {
                fprintf(stderr, ERROR "Failed to create app func.\n" CRESET);
                return NULL;
            }

            Expression *arg = _ast_to_expression(ast->value.app.arg, context, letbindings, tac_env);
            if (!arg) {
                fprintf(stderr, ERROR "Failed to create app arg.\n" CRESET);
                return NULL;
            }

            return kernel_app_create(func, arg, context);
        }

        case AST_LET: {
            Expression *type =
                _ast_to_expression(ast->value.let.type, context, letbindings, tac_env);
            if (!type) {
                fprintf(stderr, ERROR "Failed to create let type.\n" CRESET);
                return NULL;
            }

            Expression *value =
                _ast_to_expression(ast->value.let.value, context, letbindings, tac_env);
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

            return _ast_to_expression(ast->value.let.body, context, letbindings, tac_env);
        }

        case AST_MATCH: {
            Expression *scrutinee =
                _ast_to_expression(ast->value.match.scrutinee, context, letbindings, tac_env);
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

                    /* For parametric inductives, the first few foralls in the
                     * constructor type are type-parameter binders (e.g., A in
                     * sl_cons : ∀(A:Type). ∀(h:A). ...).  When matching on a
                     * scrutinee of type (sep_list A_fix), the type argument A_fix
                     * is already determined by the scrutinee.  We extract the type
                     * arguments from the scrutinee's type so that subsequent pattern
                     * variables get types that reference A_fix (via a delta-reducible
                     * alias), making `comb h` type-check correctly. */
                    Expression *scrutinee_type = kernel_expr_type(scrutinee);
                    DoublyLinkedList *type_args = dll_create();
                    {
                        Expression *head = scrutinee_type;
                        while (head != NULL && kernel_app_func(head) != NULL) {
                            dll_insert_at_head(type_args, dll_new_node(kernel_app_arg(head)));
                            head = kernel_app_func(head);
                        }
                    }
                    int scrutinee_param_count = dll_len(type_args);

                    for (int j = 0; j < pattern_var_count; j++) {
                        Expression *bound_var = kernel_forall_var(ctor_type);
                        if (!bound_var) {
                            fprintf(stderr, ERROR
                                    "Constructor has fewer parameters than pattern.\n" CRESET);
                            dll_destroy(type_args);
                            free(pattern_variables);
                            free(branches);
                            return NULL;
                        }

                        Expression *pattern_var;
                        if (j < scrutinee_param_count) {
                            /* Type-parameter slot: create a delta-reducible alias that
                             * expands to the corresponding type argument from the
                             * scrutinee. This makes the alias convertible with type_arg
                             * so later pattern variables' types unify with the outer type
                             * parameter used by functions like `comb`. */
                            Expression *type_arg = (Expression *)dll_at(type_args, j)->data;
                            pattern_var = kernel_var_create_with_body(pattern->argument_names[j],
                                                                      type_arg, extended_context);
                        } else {
                            Expression *param_type = kernel_expr_type(bound_var);
                            pattern_var = kernel_var_create(pattern->argument_names[j], param_type,
                                                            extended_context);
                        }

                        pattern_variables[j] = pattern_var;
                        extended_context = pattern_var;
                        ctor_type = kernel_forall_body(ctor_type);

                        /* Substitute bound_var -> pattern_var in the remaining constructor type.
                         * Essential for parametric constructors where later arg types
                         * reference earlier bound vars (type parameters). */
                        if (pattern_var && ctor_type) {
                            Expression *subst_ctor =
                                kernel_subst(extended_context, ctor_type, bound_var, pattern_var);
                            if (subst_ctor) {
                                ctor_type = subst_ctor;
                            }
                        }
                    }
                    dll_destroy(type_args);

                    Expression *body = _ast_to_expression(branch_ast->value.matchbranch.body,
                                                          extended_context, letbindings, tac_env);
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
                                                          context, letbindings, tac_env);
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
                                                                 contexts[i], letbindings, tac_env);
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

                Expression *return_type = _ast_to_expression(
                    ast->value.fix.return_type, contexts[binder_count], letbindings, tac_env);
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
                                                                 contexts[i], letbindings, tac_env);
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

                Expression *body = _ast_to_expression(ast->value.fix.body, contexts[binder_count],
                                                      letbindings, tac_env);
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
    Expression *result = _ast_to_expression(ast, context, letbindings, NULL);

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

Expression *ast_to_expression_env(AST *ast, Context *context, TacticEnvEntry *env) {
    if (!ast || !context) {
        return NULL;
    }

    DoublyLinkedList *letbindings = dll_create();
    Expression *result = _ast_to_expression(ast, context, letbindings, env);

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
