#include "src/commandlanguage/command_exec.h"

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
    c = context_insert(c, ind_var); // Add to context for constructor types

    // Update all contexts to include the inductive type for constructor wrapping
    for (size_t i = 0; i <= param_count; i++) {
        contexts[i] = context_insert(contexts[i], ind_var);
    }

    printf("Inductive %s : %s defined.\n", name, stringify_expression(ind_type));

    // Add each constructor to the context
    for (size_t i = 0; i < ind_cmd->constructor_count; i++) {
        InductiveConstructor *ctor = ind_cmd->constructors[i];

        Expression *ctor_type = ast_to_expression(ctor->type, c);

        Expression *full_ctor_type = ctor_type;
        for (size_t j = param_count; j > 0; j--) {
            full_ctor_type = init_forall_expression_wc(param_vars[j-1], full_ctor_type, contexts[j]);
        }

        Expression *ctor_var = init_var_expression_wc(ctor->name, full_ctor_type, rt->ctx);
        rt->ctx = context_insert(rt->ctx, ctor_var);
    }

    // Generate eliminators (_ind for Prop, _rect for Type)

    printf("  %s_ind : <eliminator for Prop>\n", name);
    printf("  %s_rect : <eliminator for Type>\n", name);
    printf("Warning: Eliminator generation not yet implemented.\n");

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
