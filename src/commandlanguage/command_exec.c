#include "src/commandlanguage/command_exec.h"

#include "src/commandlanguage/command_parser.h"
#include "src/common/color.h"
#include "src/common/options.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/inductive.h"
#include "src/kernel/normalize.h"
#include "src/kernel/utils.h"
#include "src/runtime/proof_state.h"
#include "src/runtime/runtime.h"
#include "src/termlanguage/ast_to_expression.h"

static int _handle_declaration_command(MEngineRuntime *rt, DeclarationCmd *decl_cmd) {
    Expression *var_type = ast_to_expression(decl_cmd->binder.type, rt->ctx);
    if (!var_type) {
        fprintf(stderr, ERROR "Failed to convert type for declaration '%s'.\n" CRESET,
                decl_cmd->binder.name);
        return 1;
    }
    Expression *new_var = init_var_expression_wc(decl_cmd->binder.name, var_type, rt->ctx);
    if (!new_var) {
        fprintf(stderr,
                ERROR
                "Failed to create variable '%s' (invalid type or "
                "context).\n" CRESET,
                decl_cmd->binder.name);
        return 1;
    }
    rt->ctx = new_var;

    MPRINT(rt->options->quiet, stdout, UI "%s " CRESET "%s : %s declared.\n",
           decl_keyword_to_string(decl_cmd->kw), decl_cmd->binder.name,
           stringify_expression(var_type));
    return 0;
}

Expression *_create_definition_body(MEngineRuntime *rt, Binder **params, size_t param_count,
                                    AST *body, Expression **rendered_def_body,
                                    Context **rendered_type_ctx) {
    // Given params = {(p0: P0) (p1: P1) ... (pn: Pn)}, param_count = n, and a
    // body, convert it to the term fun (p0: P0) => ... => fun (pn: Pn) => body
    Context *c = rt->ctx;
    Expression **params_rendered = malloc(param_count * sizeof(Expression *));
    Context **contexts = malloc((param_count + 1) * sizeof(Context *));

    // The entire body must be valid in the runtime context
    contexts[0] = c;

    for (size_t i = 0; i < param_count; i++) {
        Binder *b = params[i];

        Expression *param_type = ast_to_expression(b->type, c);
        Expression *param_var = init_var_expression_wc(b->name, param_type, c);
        c = param_var;

        params_rendered[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *result = ast_to_expression(body, c);
    *rendered_def_body = result;
    *rendered_type_ctx = c;
    for (size_t i = param_count; i > 0; i--) {
        result = init_lambda_expression_wc(params_rendered[i - 1], result);
    }

    free(params_rendered);
    free(contexts);
    return result;
}

static int _handle_definition_command(MEngineRuntime *rt, DefinitionCmd *defn_cmd) {
    if (!rt || !defn_cmd) {
        return 1;
    }

    const char *name = defn_cmd->name;
    Binder **params = defn_cmd->params;
    size_t param_count = defn_cmd->param_count;

    Expression *rendered_def_body = NULL;
    Context *rendered_type_ctx = NULL;
    Expression *body = _create_definition_body(rt, params, param_count, defn_cmd->body,
                                               &rendered_def_body, &rendered_type_ctx);

    Expression *inferred_type_def_body = get_expression_type(rendered_def_body);
    Expression *expected_type_def_body = ast_to_expression(defn_cmd->type, rendered_type_ctx);
    if (!expected_type_def_body) {
        fprintf(stderr, ERROR "Error:" CRESET " definition '%s' has a invalid type.\n", name);
    }

    if (!congruence(inferred_type_def_body, expected_type_def_body)) {
        fprintf(stderr, ERROR "Type error:" CRESET " definition '%s' has a mismatched type.\n",
                name);
        fprintf(stderr, "  Declared type: %s\n", stringify_expression(expected_type_def_body));
        fprintf(stderr, "  Inferred type: %s\n", stringify_expression(inferred_type_def_body));
        return 1;
    }

    Expression *defn_var = init_var_expression_wc_with_body(defn_cmd->name, body, rt->ctx);
    rt->ctx = defn_var;
    MPRINT(rt->options->quiet, stdout, UI "Definition " CRESET "%s : %s defined.\n", name,
           stringify_expression(get_expression_type(defn_var)));
    return 0;
}

static int _handle_statement_command(MEngineRuntime *rt, StatementCmd *stmt_cmd) {
    if (!rt || !stmt_cmd) {
        return 1;
    }

    Expression *statement_type = ast_to_expression(stmt_cmd->type, rt->ctx);
    if (!statement_type) {
        fprintf(stderr, ERROR "Failed to convert type for Statement %s\n" CRESET, stmt_cmd->name);
        return 1;
    }

    Expression *initial_goal = init_hole_expression("Goal", statement_type, rt->ctx);
    Expression *pending_theorem =
        init_var_expression_wc_with_body(stmt_cmd->name, initial_goal, rt->ctx);
    mengine_runtime_proof_mode(rt, pending_theorem);

    MPRINT(rt->options->quiet, stdout, UI "%s " CRESET "%s : %s stated.\n",
           stmt_keyword_to_string(stmt_cmd->kw), stmt_cmd->name,
           stringify_expression(statement_type));

    debug_print_mode(rt);
    return 0;
}

static int _handle_check_command(MEngineRuntime *rt, CheckCmd *check_cmd) {
    if (!rt || !check_cmd) {
        return 1;
    }

    // If we're in proof mode, we should render this Expression against the
    // current goal's context rather than the runtime context.
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? get_expression_context(proof_state_current(rt->proof_state))
                       : rt->ctx;
    Expression *expr = ast_to_expression(check_cmd->term, ctx);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Check command.\n" CRESET);
        return 1;
    }

    Expression *expr_type = get_expression_type(expr);
    if (!expr_type) {
        fprintf(stderr, ERROR "Failed to get type of expression in Check command.\n" CRESET);
        return 1;
    }

    MPRINT(rt->options->quiet, stdout, DIMTEXT "%s\n\t: %s\n" CRESET, stringify_expression(expr),
           stringify_expression(expr_type));
    return 0;
}

void _print_inductive_definition(MEngineRuntime *rt, Expression *expr) {
    if (!expr) {
        return;
    }

    // Inductive <name> : <type> :=
    MPRINT(rt->options->quiet, stdout,
           DIMTEXT "Inductive " CRESET "%s : %s := ", stringify_expression(expr),
           stringify_expression(expr->type));

    // | cons : type
    int constructor_count;
    Expression **constructors = get_constructors(expr, &constructor_count);
    for (int i = 0; i < constructor_count; i++) {
        Expression *ctor = constructors[i];
        MPRINT(rt->options->quiet, stdout, "\n\t| %s : %s", stringify_expression(ctor),
               stringify_expression(ctor->type));
    }

    MPRINT(rt->options->quiet, stdout, ".\n")
}

static int _handle_print_command(MEngineRuntime *rt, PrintCmd *print_cmd) {
    if (!rt || !print_cmd) {
        return 1;
    }

    // If we're in proof mode, we should render this Expression against the
    // current goal's context rather than the runtime context.
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? get_expression_context(proof_state_current(rt->proof_state))
                       : rt->ctx;
    Expression *expr = context_lookup_by_name(ctx, print_cmd->name);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Print command.\n" CRESET);
        return 1;
    }

    Expression *expr_type = get_expression_type(expr);
    if (!expr_type) {
        fprintf(stderr, ERROR "Failed to get type of expression in Print command.\n" CRESET);
        return 1;
    }

    // Check if it's an inductive type
    if (is_inductive(expr)) {
        _print_inductive_definition(rt, expr);
        return 0;
    }

    // Attempt to get the variable's body
    Expression *expr_body = get_expression_body(expr);
    if (!expr_body) {
        fprintf(stderr, ERROR "%s is an opaque variable.\n" CRESET, stringify_expression(expr));
        return 1;
    }

    MPRINT(rt->options->quiet, stdout, DIMTEXT "%s := %s\n\t: %s\n" CRESET,
           stringify_expression(expr), stringify_expression(expr_body),
           stringify_expression(expr_type));
    return 0;
}

static Expression *_build_constructor_case_type(Expression *ctor_expr, Expression *ctor_type,
                                                Expression *motive_var, Expression **param_vars,
                                                size_t param_count, size_t index_count,
                                                Context *elim_ctx);

static Expression *_build_motive_type(Expression *ind_var, Expression **param_vars,
                                      size_t param_count, Context *ctx,
                                      Expression ***index_vars_out, size_t *index_count_out) {
    Expression *ind_applied = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = init_app_expression_wc(ind_applied, param_vars[i], ctx);
        if (!ind_applied) {
            fprintf(stderr, ERROR "Failed to apply parameter %zu in motive type\n" CRESET, i);
            return NULL;
        }
    }

    // Check if the TYPE of ind_applied is a forall - if so, we have indices
    Expression *ind_applied_type = get_expression_type(ind_applied);
    Expression *current = ind_applied_type;
    size_t index_count = 0;
    while (current->tag == FORALL_EXPRESSION) {
        index_count++;
        current = get_forall_body(current);
    }

    if (index_count == 0) {
        // No indices: motive is (ind params) -> Prop
        *index_vars_out = NULL;
        *index_count_out = 0;
        return init_arrow_expression_wc(ind_applied, init_prop_expression(), ctx);
    }

    // With indices: motive is forall (indices), (ind params indices) -> Prop
    Expression **index_vars = malloc(index_count * sizeof(Expression *));
    Context *motive_ctx = ctx;
    current = ind_applied_type;

    for (size_t i = 0; i < index_count; i++) {
        Expression *index_type = get_expression_type(get_forall_bound_variable(current));
        char index_name[32];
        sprintf(index_name, "i%zu", i);

        index_vars[i] = init_var_expression_wc(index_name, index_type, motive_ctx);
        motive_ctx = index_vars[i];
        current = get_forall_body(current);
    }

    // Build (ind params index_0 ... index_n)
    Expression *ind_with_indices = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_with_indices = init_app_expression_wc(ind_with_indices, param_vars[i], motive_ctx);
    }
    for (size_t i = 0; i < index_count; i++) {
        ind_with_indices = init_app_expression_wc(ind_with_indices, index_vars[i], motive_ctx);
    }

    // Build forall (i0 : T0) ... (in : Tn), (ind params i0 ... in) -> Prop
    Expression *motive_type =
        init_arrow_expression_wc(ind_with_indices, init_prop_expression(), motive_ctx);
    for (size_t i = index_count; i > 0; i--) {
        motive_type = init_forall_expression_wc(index_vars[i - 1], motive_type);
        motive_ctx = get_expression_context(index_vars[i - 1]);
    }

    *index_vars_out = index_vars;
    *index_count_out = index_count;
    return motive_type;
}

static Expression *_build_induction_principle_type(InductiveCmd *ind_cmd, Expression *ind_var,
                                                   Expression **param_vars, size_t param_count,
                                                   Context **contexts) {
    Context *elim_ctx = contexts[param_count];

    Expression **index_vars = NULL;
    size_t index_count = 0;
    Expression *motive_type =
        _build_motive_type(ind_var, param_vars, param_count, elim_ctx, &index_vars, &index_count);
    Expression *motive_var = init_var_expression_wc("P", motive_type, elim_ctx);
    elim_ctx = motive_var;

    size_t ctor_count = ind_cmd->constructor_count;
    Expression **case_vars = malloc(ctor_count * sizeof(Expression *));
    Context **case_contexts = malloc((ctor_count + 1) * sizeof(Context *));
    case_contexts[0] = elim_ctx;

    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];
        Expression *ctor_expr = context_lookup_by_name(elim_ctx, ctor->name);
        if (!ctor_expr) {
            fprintf(stderr, ERROR "Constructor %s not found in context\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        Expression *ctor_type = get_expression_type(ctor_expr);
        Expression *case_type =
            _build_constructor_case_type(ctor_expr, ctor_type, motive_var, param_vars, param_count,
                                         index_count, case_contexts[i]);

        if (!case_type) {
            fprintf(stderr, ERROR "Failed to build case type for %s\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        char case_name[64];
        sprintf(case_name, "case_%s", ctor->name);
        case_vars[i] = init_var_expression_wc(case_name, case_type, case_contexts[i]);
        if (!case_vars[i]) {
            fprintf(stderr, ERROR "Failed to create case variable for %s\n" CRESET, ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
        case_contexts[i + 1] = case_vars[i];
    }

    // Build conclusion: forall (indices) (target : ind params indices), P
    // indices target
    Context *final_ctx = case_contexts[ctor_count];

    // Create fresh index variables for the conclusion
    Expression **concl_index_vars = NULL;
    if (index_count > 0) {
        concl_index_vars = malloc(index_count * sizeof(Expression *));
        for (size_t i = 0; i < index_count; i++) {
            Expression *index_type = get_expression_type(index_vars[i]);
            char index_name[32];
            sprintf(index_name, "y%zu", i);
            concl_index_vars[i] = init_var_expression_wc(index_name, index_type, final_ctx);
            final_ctx = concl_index_vars[i];
        }
    }

    // Build (ind params concl_index_0 ... concl_index_n)
    Expression *ind_applied = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = init_app_expression_wc(ind_applied, param_vars[i], final_ctx);
        if (!ind_applied) {
            fprintf(stderr,
                    ERROR
                    "Failed to apply parameter %zu to inductive in "
                    "conclusion\n" CRESET,
                    i);
            free(case_vars);
            free(case_contexts);
            if (index_vars) {
                free(index_vars);
            }
            if (concl_index_vars) {
                free(concl_index_vars);
            }
            return NULL;
        }
    }
    for (size_t i = 0; i < index_count; i++) {
        ind_applied = init_app_expression_wc(ind_applied, concl_index_vars[i], final_ctx);
    }

    Expression *target_var = init_var_expression_wc("e", ind_applied, final_ctx);
    Context *target_ctx = target_var;

    // Apply motive to indices and target: P concl_index_0 ... concl_index_n
    // target
    Expression *target_applied = motive_var;
    for (size_t i = 0; i < index_count; i++) {
        target_applied = init_app_expression_wc(target_applied, concl_index_vars[i], target_ctx);
    }
    target_applied = init_app_expression_wc(target_applied, target_var, target_ctx);
    if (!target_applied) {
        fprintf(stderr, ERROR "Failed to apply motive to target\n" CRESET);
        free(case_vars);
        free(case_contexts);
        if (index_vars) {
            free(index_vars);
        }
        if (concl_index_vars) {
            free(concl_index_vars);
        }
        return NULL;
    }

    Expression *result = init_forall_expression_wc(target_var, target_applied);

    // Wrap with foralls for conclusion indices
    for (size_t i = index_count; i > 0; i--) {
        result = init_forall_expression_wc(concl_index_vars[i - 1], result);
        final_ctx = get_expression_context(concl_index_vars[i - 1]);
    }

    if (concl_index_vars) {
        free(concl_index_vars);
    }

    for (size_t i = ctor_count; i > 0; i--) {
        result = init_forall_expression_wc(case_vars[i - 1], result);
        if (!result) {
            fprintf(stderr, ERROR "Failed to wrap with constructor case %zu\n" CRESET, i - 1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    result = init_forall_expression_wc(motive_var, result);
    if (!result) {
        fprintf(stderr, ERROR "Failed to wrap with motive P\n" CRESET);
        free(case_vars);
        free(case_contexts);
        return NULL;
    }

    for (size_t i = param_count; i > 0; i--) {
        result = init_forall_expression_wc(param_vars[i - 1], result);
        if (!result) {
            fprintf(stderr, ERROR "Failed to wrap with parameter %zu\n" CRESET, i - 1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    free(case_vars);
    free(case_contexts);
    if (index_vars) {
        free(index_vars);
    }
    return result;
}

static Expression *_build_constructor_case_type(Expression *ctor_expr, Expression *ctor_type,
                                                Expression *motive_var, Expression **param_vars,
                                                size_t param_count, size_t index_count,
                                                Context *elim_ctx) {
    Expression *core_type = ctor_type;
    for (size_t i = 0; i < param_count; i++) {
        if (core_type->tag == FORALL_EXPRESSION) {
            core_type = get_forall_body(core_type);
        }
    }

    DoublyLinkedList *arg_types = dll_create();
    Expression *current = core_type;
    while (current->tag == FORALL_EXPRESSION) {
        Expression *arg_type = get_expression_type(get_forall_bound_variable(current));
        dll_insert_at_tail(arg_types, dll_new_node(arg_type));
        current = get_forall_body(current);
    }

    Expression *ctor_app = ctor_expr;
    for (size_t i = 0; i < param_count; i++) {
        ctor_app = init_app_expression_wc(ctor_app, param_vars[i], elim_ctx);
        if (!ctor_app) {
            fprintf(stderr, ERROR "Failed to apply parameter %zu to constructor\n" CRESET, i);
            dll_destroy(arg_types);
            return NULL;
        }
    }

    size_t arg_count = dll_len(arg_types);
    Expression **arg_vars = malloc(arg_count * sizeof(Expression *));
    Context *case_ctx = elim_ctx;

    for (size_t i = 0; i < arg_count; i++) {
        Expression *arg_type = (Expression *)dll_at(arg_types, i)->data;
        char arg_name[32];
        sprintf(arg_name, "arg%zu", i);

        arg_vars[i] = init_var_expression_wc(arg_name, arg_type, case_ctx);
        case_ctx = arg_vars[i];
        ctor_app = init_app_expression_wc(ctor_app, arg_vars[i], case_ctx);
        if (!ctor_app) {
            fprintf(stderr, ERROR "Failed to apply constructor arg %zu\n" CRESET, i);
            dll_destroy(arg_types);
            free(arg_vars);
            return NULL;
        }
    }

    // Extract indices from constructor return type
    // current holds the return type after stripping foralls
    Expression **ctor_indices = NULL;
    if (index_count > 0) {
        ctor_indices = malloc(index_count * sizeof(Expression *));

        // Parse the return type as a spine of applications
        // For eq_refl: (((eq A) x) x) - we want to extract the indices (the
        // trailing applications)
        DoublyLinkedList *spine = dll_create();
        Expression *head = current;
        while (head->tag == APP_EXPRESSION) {
            dll_insert_at_head(spine, dll_new_node(get_app_arg(head)));
            head = get_app_func(head);
        }

        // The spine now has all the arguments. Skip param_count, take
        // index_count
        size_t total_args = dll_len(spine);
        if (total_args < param_count + index_count) {
            fprintf(stderr, ERROR "Constructor return type has too few arguments\n" CRESET);
            dll_destroy(spine);
            dll_destroy(arg_types);
            free(arg_vars);
            free(ctor_indices);
            return NULL;
        }

        // Extract the indices (skip parameters)
        for (size_t i = 0; i < index_count; i++) {
            ctor_indices[i] = (Expression *)dll_at(spine, param_count + i)->data;
        }

        dll_destroy(spine);
    }

    // Apply motive to indices first, then to constructor application
    Expression *case_result = motive_var;
    for (size_t i = 0; i < index_count; i++) {
        case_result = init_app_expression_wc(case_result, ctor_indices[i], case_ctx);
        if (!case_result) {
            fprintf(stderr, ERROR "Failed to apply motive to index %zu\n" CRESET, i);
            dll_destroy(arg_types);
            free(arg_vars);
            if (ctor_indices) {
                free(ctor_indices);
            }
            return NULL;
        }
    }

    case_result = init_app_expression_wc(case_result, ctor_app, case_ctx);
    if (!case_result) {
        fprintf(stderr, ERROR "Failed to apply motive to constructor application\n" CRESET);
        dll_destroy(arg_types);
        free(arg_vars);
        if (ctor_indices) {
            free(ctor_indices);
        }
        return NULL;
    }

    if (ctor_indices) {
        free(ctor_indices);
    }
    Expression *case_type = case_result;
    for (size_t i = arg_count; i > 0; i--) {
        case_type = init_forall_expression_wc(arg_vars[i - 1], case_type);
    }

    dll_destroy(arg_types);
    free(arg_vars);
    return case_type;
}

static int _handle_inductive_command(MEngineRuntime *rt, InductiveCmd *ind_cmd) {
    if (!rt || !ind_cmd) {
        return 1;
    }

    const char *name = ind_cmd->name;
    Binder **params = ind_cmd->params;
    size_t param_count = ind_cmd->param_count;

    Context *c = rt->ctx;
    Expression **param_vars = malloc(param_count * sizeof(Expression *));
    Context **contexts = malloc((param_count + 1) * sizeof(Context *));

    contexts[0] = c;

    for (size_t i = 0; i < param_count; i++) {
        Expression *param_type = ast_to_expression(params[i]->type, c);
        Expression *param_var = init_var_expression_wc(params[i]->name, param_type, c);
        c = param_var;

        param_vars[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *ind_return_type = ast_to_expression(ind_cmd->type, c);
    Expression *ind_type = ind_return_type;
    for (size_t i = param_count; i > 0; i--) {
        ind_type = init_forall_expression_wc(param_vars[i - 1], ind_type);
    }

    Expression *ind_var = init_var_expression_wc(name, ind_type, rt->ctx);
    rt->ctx = ind_var;
    c = ind_var;

    for (size_t i = 0; i <= param_count; i++) {
        contexts[i] = ind_var;
    }

    MPRINT(rt->options->quiet, stdout, UI "Inductive " CRESET "%s : %s defined.\n", name,
           stringify_expression(ind_type));

    size_t ctor_count = ind_cmd->constructor_count;

    Expression **ctor_vars = malloc(ctor_count * sizeof(Expression *));
    if (!ctor_vars) {
        fprintf(stderr, ERROR "Failed to allocate constructor array.\n" CRESET);
        free(param_vars);
        free(contexts);
        return 1;
    }

    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];

        Expression *ctor_core_type = ast_to_expression(ctor->type, c);
        if (!ctor_core_type) {
            fprintf(stderr, ERROR "Failed to convert constructor type for %s\n" CRESET, ctor->name);
            free(param_vars);
            free(contexts);
            return 1;
        }

        Expression *ctor_type = ctor_core_type;
        for (size_t j = param_count; j > 0; j--) {
            ctor_type = init_forall_expression_wc(param_vars[j - 1], ctor_type);
            if (!ctor_type) {
                fprintf(stderr,
                        ERROR
                        "Failed to wrap constructor type with parameter "
                        "%zu\n" CRESET,
                        j - 1);
                free(param_vars);
                free(contexts);
                return 1;
            }
        }

        Expression *ctor_var = init_var_expression_wc(ctor->name, ctor_type, rt->ctx);
        if (!ctor_var) {
            fprintf(stderr, ERROR "Failed to create constructor variable for %s\n" CRESET,
                    ctor->name);
            free(ctor_vars);
            free(param_vars);
            free(contexts);
            return 1;
        }

        ctor_vars[i] = ctor_var;

        rt->ctx = ctor_var;
        c = ctor_var;

        for (size_t j = 0; j <= param_count; j++) {
            contexts[j] = ctor_var;
        }

        MPRINT(rt->options->quiet, stdout, UI "Constructor " CRESET "%s : %s defined.\n",
               ctor->name, stringify_expression(ctor_type));
    }

    char *ind_principle_name = malloc(strlen(name) + 5);
    sprintf(ind_principle_name, "%s_ind", name);

    Expression *ind_principle_type =
        _build_induction_principle_type(ind_cmd, ind_var, param_vars, param_count, contexts);

    Expression *ind_principle_var = NULL;
    if (ind_principle_type) {
        ind_principle_var = init_var_expression_wc(ind_principle_name, ind_principle_type, rt->ctx);
        rt->ctx = ind_principle_var;

        MPRINT(rt->options->quiet, stdout, UI "Induction principle " CRESET "%s : %s generated.\n",
               ind_principle_name, stringify_expression(ind_principle_type));
    }

    // Register the inductive type
    if (!register_inductive(ind_var, ctor_vars, ctor_count, ind_principle_var)) {
        fprintf(stderr, ERROR "Failed to register inductive type %s.\n" CRESET, name);
    } else {
        MPRINT(rt->options->quiet, stdout,
               UI "Registered " CRESET "inductive %s with %zu constructor(s).\n", name, ctor_count);
    }

    free(ctor_vars);
    free(ind_principle_name);
    free(param_vars);
    free(contexts);
    return 0;
}

static int _handle_fixpoint_command(MEngineRuntime *rt, FixpointCmd *fix_cmd) {
    if (!rt || !fix_cmd) {
        return 1;
    }

    // Convert the fixpoint AST to a fix expression
    AST *fix_ast = malloc(sizeof(AST));
    fix_ast->tag = AST_FIX;
    fix_ast->value.fix.name = fix_cmd->name;
    fix_ast->value.fix.binders = fix_cmd->binders;
    fix_ast->value.fix.binder_count = fix_cmd->binder_count;
    fix_ast->value.fix.decreasing_arg_name = fix_cmd->decreasing_arg_name;
    fix_ast->value.fix.return_type = fix_cmd->return_type;
    fix_ast->value.fix.body = fix_cmd->body;

    Expression *fixpoint_var = ast_to_expression(fix_ast, rt->ctx);
    if (!fixpoint_var) {
        fprintf(stderr, ERROR "Failed to convert fixpoint '%s' to expression.\n" CRESET,
                fix_cmd->name);
        return 1;
    }

    rt->ctx = fixpoint_var;

    MPRINT(rt->options->quiet, stdout, UI "Fixpoint " CRESET "%s : %s defined.\n", fix_cmd->name,
           stringify_expression(get_expression_type(fixpoint_var)));
    return 0;
}

static int _handle_eval_command(MEngineRuntime *rt, EvalCmd *eval_cmd) {
    if (!rt || !eval_cmd) {
        return 1;
    }

    // Convert AST to expression
    Context *ctx = (rt->mode == MENGINE_RUNTIME_PROOF_MODE)
                       ? get_expression_context(proof_state_current(rt->proof_state))
                       : rt->ctx;
    Expression *expr = ast_to_expression(eval_cmd->term, ctx);
    if (!expr) {
        fprintf(stderr, ERROR "Failed to convert term in Eval command.\n" CRESET);
        return 1;
    }

    // Determine reduction flags based on strategy and flags
    ReductionFlags flags = 0;
    Expression *result = NULL;

    switch (eval_cmd->strategy) {
        case EVAL_STRATEGY_CBV:
            // For cbv, use the flags if specified, otherwise use all reductions
            if (eval_cmd->beta_flag || eval_cmd->delta_flag || eval_cmd->iota_flag ||
                eval_cmd->fix_flag) {
                if (eval_cmd->beta_flag) {
                    flags |= REDUCE_BETA;
                }
                if (eval_cmd->delta_flag) {
                    flags |= REDUCE_DELTA;
                }
                if (eval_cmd->iota_flag) {
                    flags |= REDUCE_IOTA;
                }
                if (eval_cmd->fix_flag) {
                    flags |= REDUCE_FIX;
                }
            } else {
                flags = REDUCE_ALL;
            }
            result = normalize_cbv(expr, flags);
            break;

        case EVAL_STRATEGY_COMPUTE:
            result = normalize_compute(expr);
            break;

        default:
            fprintf(stderr, ERROR "Unknown evaluation strategy.\n" CRESET);
            return 1;
    }

    if (!result) {
        fprintf(stderr, ERROR "Normalization failed.\n" CRESET);
        return 1;
    }

    MPRINT(rt->options->quiet, stdout, DIMTEXT "\t= %s\n\t: %s\n" CRESET,
           stringify_expression(result), stringify_expression(get_expression_type(result)));
    return 0;
}

static int _handle_show_command(MEngineRuntime *rt, ShowCmd *show_cmd) {
    if (!rt || !show_cmd) {
        return 1;
    }

    switch (show_cmd->kw) {
        case SHOW_KW_CONTEXT: {
            Context *ctx = mengine_runtime_context(rt);
            MPRINT(rt->options->quiet, stdout, HEADER "Context:" CRESET "\n%s\n",
                   stringify_context(ctx, CTX_STRINGIFY_PRETTY_IND0));
            break;
        }
        case SHOW_KW_PROOF: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show Proof' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = proof_state_current(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            // Show proof term
            Expression *pending_theorem = proof_state_pending_theorem(rt->proof_state);
            Expression *pending_theorem_body = get_expression_body(pending_theorem);
            Expression *pending_theorem_type = get_expression_type(pending_theorem);
            MPRINT(rt->options->quiet, stdout, HEADER "Proof Term:" CRESET "\n%s : %s\n",
                   stringify_expression(pending_theorem_body),
                   stringify_expression(pending_theorem_type));
            return 0;
        }
        case SHOW_KW_GOAL: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show Goal' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = proof_state_current(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            Context *goal_ctx = get_expression_context(current_goal);
            Context *runtime_ctx = mengine_runtime_context(rt);
            if (goal_ctx && runtime_ctx) {
                char *ctx_str =
                    stringify_context_until(goal_ctx, runtime_ctx, CTX_STRINGIFY_PRETTY_IND0);
                MPRINT(rt->options->quiet, stdout, HEADER "Goal Context:" CRESET "\n%s\n", ctx_str);
                free(ctx_str);
            }
            MPRINT(rt->options->quiet, stdout, HEADER "Goal:" CRESET "\n%s\n",
                   stringify_expression(get_expression_type(current_goal)));
            return 0;
        }
        case SHOW_KW_STATE: {
            if (rt->mode == MENGINE_RUNTIME_COMMAND_MODE) {
                fprintf(stderr, RED
                        "Error: 'Show State' can only be used in Proof "
                        "Mode.\n" CRESET);
                return 1;
            }

            if (!rt->proof_state) {
                fprintf(stderr, ERROR " No active proof state.\n" CRESET);
                return 1;
            }

            Expression *current_goal = proof_state_current(rt->proof_state);
            if (!current_goal) {
                fprintf(stderr, ERROR " No current goal.\n" CRESET);
                return 1;
            }

            // Show proof term
            Expression *pending_theorem = proof_state_pending_theorem(rt->proof_state);
            Expression *pending_theorem_body = get_expression_body(pending_theorem);
            MPRINT(rt->options->quiet, stdout, HEADER "Proof Term:" CRESET "\n%s\n",
                   stringify_expression(pending_theorem_body));

            // Show current goal context
            Context *goal_ctx = get_expression_context(current_goal);
            Context *runtime_ctx = mengine_runtime_context(rt);
            if (goal_ctx && runtime_ctx) {
                char *ctx_str =
                    stringify_context_until(goal_ctx, runtime_ctx, CTX_STRINGIFY_PRETTY_IND0);
                MPRINT(rt->options->quiet, stdout, CYN "Goal Context:" CRESET "\n%s\n", ctx_str);
                free(ctx_str);
            }

            // Show goal
            MPRINT(rt->options->quiet, stdout, CYN "Goal:" CRESET "\n%s\n",
                   stringify_expression(get_expression_type(current_goal)));
            return 0;
        }
    }
    return 0;
}

int mengine_execute_command(MEngineRuntime *rt, Command *cmd) {
    if (!rt || !cmd) {
        return 1;
    }

    switch (cmd->tag) {
        case CMD_DECLARATION: {
            return _handle_declaration_command(rt, &cmd->as.decl);
        }
        case CMD_DEFINITION: {
            return _handle_definition_command(rt, &cmd->as.defn);
        }
        case CMD_STATEMENT: {
            return _handle_statement_command(rt, &cmd->as.stmt);
        }
        case CMD_CHECK: {
            return _handle_check_command(rt, &cmd->as.check);
        }
        case CMD_PRINT: {
            return _handle_print_command(rt, &cmd->as.print);
        }
        case CMD_EVAL: {
            return _handle_eval_command(rt, &cmd->as.eval);
        }
        case CMD_INDUCTIVE: {
            return _handle_inductive_command(rt, &cmd->as.inductive);
        }
        case CMD_FIXPOINT: {
            return _handle_fixpoint_command(rt, &cmd->as.fixpoint);
        }
        case CMD_SHOW: {
            return _handle_show_command(rt, &cmd->as.show);
        }
        default:
            return 1;
    }
}
