#include "src/commandlanguage/command_exec.h"
#include "src/kernel/expression.h"

static void _handle_declaration_command(MEngineRuntime *rt,
                                        DeclarationCmd *decl_cmd) {
    Expression *var_type = ast_to_expression(decl_cmd->binder.type, rt->ctx);
    Expression *new_var =
        init_var_expression_wc(decl_cmd->binder.name, var_type, rt->ctx);
    rt->ctx = context_insert(rt->ctx, new_var);

    printf("%s %s : %s declared.\n", decl_keyword_to_string(decl_cmd->kw),
           decl_cmd->binder.name, stringify_expression(var_type));
    return;
}

Expression *_create_definition_body(MEngineRuntime *rt, Binder **params,
                                    size_t param_count, AST *body,
                                    Expression **rendered_def_body) {
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
        c = context_insert(c, param_var);

        params_rendered[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *result = ast_to_expression(body, c);
    *rendered_def_body = result;
    for (size_t i = param_count - 1; i >= 0; i--) {
        result =
            init_lambda_expression_wc(params_rendered[i], result, contexts[i]);
    }

    free(params_rendered);
    free(contexts);
    return result;
}

static void _handle_definition_command(MEngineRuntime *rt,
                                       DefinitionCmd *defn_cmd) {
    if (!rt || !defn_cmd) return;

    const char *name = defn_cmd->name;
    Binder **params = defn_cmd->params;
    size_t param_count = defn_cmd->param_count;

    Expression *rendered_def_body = NULL;
    Expression *body = _create_definition_body(
        rt, params, param_count, defn_cmd->type, &rendered_def_body);

    Expression *inferred_type_def_body = get_expression_type(rendered_def_body);
    Expression *expected_type_def_body =
        ast_to_expression(defn_cmd->type, rt->ctx);
    if (!congruence(inferred_type_def_body, expected_type_def_body)) {
        fprintf(stderr,
                RED "Type error:" CRESET
                    " definition '%s' has a mismatched type.\n",
                name);
        fprintf(stderr, "Declared type: %s\n",
                stringify_expression(expected_type_def_body));
        fprintf(stderr, "Inferred type: %s\n",
                stringify_expression(inferred_type_def_body));
        return;
    }

    definition_table_insert(rt->def_table, name, inferred_type_def_body, body);
    Expression *defn_var =
        init_var_expression_wc(defn_cmd->name, inferred_type_def_body, rt->ctx);
    rt->ctx = context_insert(rt->ctx, defn_var);

    printf("Definition %s : %s defined.\n", name,
           stringify_expression(inferred_type_def_body));
}

static void _handle_statement_command(MEngineRuntime *rt,
                                      StatementCmd *stmt_cmd) {
    if (!rt || !stmt_cmd) return;

    Expression *statement_type = ast_to_expression(stmt_cmd->type, rt->ctx);
    if (!statement_type) {
        fprintf(stderr, "Failed to convert type for Statement %s\n",
                stmt_cmd->name);
        return;
    }

    Expression *theorem =
        init_var_expression_wc(stmt_cmd->name, statement_type, rt->ctx);
    mengine_runtime_proof_mode(rt, theorem);

    printf("%s %s : %s stated.\n", stmt_keyword_to_string(stmt_cmd->kw),
           stmt_cmd->name, stringify_expression(statement_type));

    debug_print_mode(rt);
}

static void _handle_check_command(MEngineRuntime *rt, CheckCmd *check_cmd) {
    if (!rt || !check_cmd) return;

    Expression *expr = ast_to_expression(check_cmd->term, rt->ctx);
    if (!expr) {
        fprintf(stderr,
                "Runtime Error: Failed to convert term in Check command.\n");
        return;
    }

    Expression *expr_type = get_expression_type(expr);
    if (!expr_type) {
        fprintf(stderr,
                "Runtime Error: Failed to get type of expression in Check "
                "command.\n");
        return;
    }

    printf(GRAY "%s\n\t: %s\n" CRESET, stringify_expression(expr),
           stringify_expression(expr_type));
}

static Expression *_build_constructor_case_type(Expression *ctor_expr,
                                                 Expression *ctor_type,
                                                 Expression *motive_var,
                                                 Expression **param_vars,
                                                 size_t param_count,
                                                 Context *elim_ctx);

static Expression *_build_motive_type(Expression *ind_var,
                                       Expression **param_vars,
                                       size_t param_count,
                                       Context *ctx) {
    Expression *ind_applied = ind_var;
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = init_app_expression_wc(ind_applied, param_vars[i], ctx);
    }
    return init_arrow_expression(ind_applied, init_prop_expression());
}

static Expression *_build_induction_principle_type(InductiveCmd *ind_cmd,
                                                    Expression *ind_var,
                                                    Expression **param_vars,
                                                    size_t param_count,
                                                    Context **contexts) {
    Context *elim_ctx = contexts[param_count];
    Expression *motive_type = _build_motive_type(ind_var, param_vars, param_count, elim_ctx);
    Expression *motive_var = init_var_expression_wc("P", motive_type, elim_ctx);
    elim_ctx = context_insert(elim_ctx, motive_var);

    size_t ctor_count = ind_cmd->constructor_count;
    Expression **case_vars = malloc(ctor_count * sizeof(Expression *));
    Context **case_contexts = malloc((ctor_count + 1) * sizeof(Context *));
    case_contexts[0] = elim_ctx;

    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];
        Expression *ctor_expr = context_lookup_by_name(elim_ctx, ctor->name);
        if (!ctor_expr) {
            fprintf(stderr, "Error: Constructor %s not found in context\n", ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        Expression *ctor_type = get_expression_type(ctor_expr);
        Expression *case_type = _build_constructor_case_type(
            ctor_expr, ctor_type, motive_var, param_vars, param_count, case_contexts[i]);

        if (!case_type) {
            fprintf(stderr, "Error: Failed to build case type for %s\n", ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }

        char case_name[64];
        sprintf(case_name, "case_%s", ctor->name);
        case_vars[i] = init_var_expression_wc(case_name, case_type, case_contexts[i]);
        if (!case_vars[i]) {
            fprintf(stderr, "Error: Failed to create case variable for %s\n", ctor->name);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
        case_contexts[i + 1] = context_insert(case_contexts[i], case_vars[i]);
    }

    Expression *ind_applied = ind_var;
    Context *final_ctx = case_contexts[ctor_count];
    for (size_t i = 0; i < param_count; i++) {
        ind_applied = init_app_expression_wc(ind_applied, param_vars[i], final_ctx);
    }

    Expression *target_var = init_var_expression_wc("i", ind_applied, final_ctx);
    Context *target_ctx = context_insert(final_ctx, target_var);
    Expression *target_applied = init_app_expression_wc(motive_var, target_var, target_ctx);
    Expression *result = init_forall_expression_wc(target_var, target_applied, target_ctx);

    for (size_t i = ctor_count; i > 0; i--) {
        result = init_forall_expression_wc(case_vars[i-1], result, case_contexts[i]);
        if (!result) {
            fprintf(stderr, "Error: Failed to wrap with constructor case %zu\n", i-1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    result = init_forall_expression_wc(motive_var, result, elim_ctx);
    if (!result) {
        fprintf(stderr, "Error: Failed to wrap with motive P\n");
        free(case_vars);
        free(case_contexts);
        return NULL;
    }

    for (size_t i = param_count; i > 0; i--) {
        result = init_forall_expression_wc(param_vars[i-1], result, contexts[i]);
        if (!result) {
            fprintf(stderr, "Error: Failed to wrap with parameter %zu\n", i-1);
            free(case_vars);
            free(case_contexts);
            return NULL;
        }
    }

    free(case_vars);
    free(case_contexts);
    return result;
}

static Expression *_build_constructor_case_type(Expression *ctor_expr,
                                                 Expression *ctor_type,
                                                 Expression *motive_var,
                                                 Expression **param_vars,
                                                 size_t param_count,
                                                 Context *elim_ctx) {
    Expression *core_type = ctor_type;
    for (size_t i = 0; i < param_count; i++) {
        if (core_type->type == FORALL_EXPRESSION) {
            core_type = core_type->value.forall.body;
        }
    }

    DoublyLinkedList *arg_types = dll_create();
    Expression *current = core_type;
    while (current->type == FORALL_EXPRESSION) {
        Expression *arg_type = get_expression_type(current->value.forall.bound_variable);
        dll_insert_at_tail(arg_types, dll_new_node(arg_type));
        current = current->value.forall.body;
    }

    Expression *ctor_app = ctor_expr;
    for (size_t i = 0; i < param_count; i++) {
        ctor_app = init_app_expression(ctor_app, param_vars[i]);
    }

    size_t arg_count = dll_len(arg_types);
    Expression **arg_vars = malloc(arg_count * sizeof(Expression *));
    Context *case_ctx = elim_ctx;

    for (size_t i = 0; i < arg_count; i++) {
        Expression *arg_type = (Expression *)dll_at(arg_types, i)->data;
        char arg_name[32];
        sprintf(arg_name, "arg%zu", i);

        arg_vars[i] = init_var_expression_wc(arg_name, arg_type, case_ctx);
        case_ctx = context_insert(case_ctx, arg_vars[i]);
        ctor_app = init_app_expression(ctor_app, arg_vars[i]);
    }

    Expression *case_result = init_app_expression(motive_var, ctor_app);
    Expression *case_type = case_result;
    Context *wrap_ctx = case_ctx;
    for (size_t i = arg_count; i > 0; i--) {
        case_type = init_forall_expression_wc(arg_vars[i-1], case_type, wrap_ctx);
        wrap_ctx = context_minus(wrap_ctx, arg_vars[i-1]);
    }

    dll_destroy(arg_types);
    free(arg_vars);
    return case_type;
}

static void _handle_inductive_command(MEngineRuntime *rt, InductiveCmd *ind_cmd) {
    if (!rt || !ind_cmd) return;

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
        c = context_insert(c, param_var);

        param_vars[i] = param_var;
        contexts[i + 1] = c;
    }

    Expression *ind_return_type = ast_to_expression(ind_cmd->type, c);
    Expression *ind_type = ind_return_type;
    for (size_t i = param_count; i > 0; i--) {
        ind_type = init_forall_expression_wc(param_vars[i-1], ind_type, contexts[i-1]);
    }

    Expression *ind_var = init_var_expression_wc(name, ind_type, rt->ctx);
    rt->ctx = context_insert(rt->ctx, ind_var);
    c = context_insert(c, ind_var);

    for (size_t i = 0; i <= param_count; i++) {
        contexts[i] = context_insert(contexts[i], ind_var);
    }

    printf("Inductive %s : %s defined.\n", name, stringify_expression(ind_type));

    size_t ctor_count = ind_cmd->constructor_count;
    for (size_t i = 0; i < ctor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];

        Expression *ctor_core_type = ast_to_expression(ctor->type, c);
        if (!ctor_core_type) {
            fprintf(stderr, "Error: Failed to convert constructor type for %s\n", ctor->name);
            free(param_vars);
            free(contexts);
            return;
        }

        Expression *ctor_type = ctor_core_type;
        for (size_t j = param_count; j > 0; j--) {
            ctor_type = init_forall_expression_wc(param_vars[j-1], ctor_type, contexts[j]);
            if (!ctor_type) {
                fprintf(stderr, "Error: Failed to wrap constructor type with parameter %zu\n", j-1);
                free(param_vars);
                free(contexts);
                return;
            }
        }

        Expression *ctor_var = init_var_expression_wc(ctor->name, ctor_type, rt->ctx);
        if (!ctor_var) {
            fprintf(stderr, "Error: Failed to create constructor variable for %s\n", ctor->name);
            free(param_vars);
            free(contexts);
            return;
        }

        rt->ctx = context_insert(rt->ctx, ctor_var);
        c = context_insert(c, ctor_var);

        for (size_t j = 0; j <= param_count; j++) {
            contexts[j] = context_insert(contexts[j], ctor_var);
        }

        printf("Constructor %s : %s defined.\n", ctor->name, stringify_expression(ctor_type));
    }

    char *ind_principle_name = malloc(strlen(name) + 5);
    sprintf(ind_principle_name, "%s_ind", name);

    Expression *ind_principle_type = _build_induction_principle_type(
        ind_cmd, ind_var, param_vars, param_count, contexts);

    if (ind_principle_type) {
        Expression *ind_principle_var =
            init_var_expression_wc(ind_principle_name, ind_principle_type, rt->ctx);
        rt->ctx = context_insert(rt->ctx, ind_principle_var);

        printf("Induction principle %s : %s generated.\n",
               ind_principle_name, stringify_expression(ind_principle_type));
    }

    free(ind_principle_name);
    free(param_vars);
    free(contexts);
}

void mengine_execute_command(MEngineRuntime *rt, Command *cmd) {
    if (!rt || !cmd) {
        return;
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
        case CMD_INDUCTIVE: {
            return _handle_inductive_command(rt, &cmd->as.inductive);
        }
        case CMD_DECL_KEYWORD:
        case CMD_STMT_KEYWORD:
        default:
            return;
    }
}
