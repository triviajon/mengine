#include "src/runtime/runtime.h"

void debug_print_mode_update(MEngineRuntime *rt) {
    if (!rt || !rt->options) return;
    if (!rt->options->debug || !rt->options->debug__print_mode) return;

    fprintf(stderr, MAG "[MODE]" DIM " Runtime mode changed to ");

    switch (rt->mode) {
        case MENGINE_RUNTIME_COMMAND_MODE:
            fprintf(stderr, "COMMAND_MODE");
            break;

        case MENGINE_RUNTIME_PROOF_MODE:
            fprintf(stderr, "PROOF_MODE");
            if (rt->pending_theorem) {
                fprintf(stderr, " (goal: %s)",
                        stringify_expression(rt->pending_theorem));
            }
            break;

        default:
            fprintf(stderr, "UNKNOWN_MODE");
            break;
    }

    fprintf(stderr, CRESET "\n");
}

MEngineRuntime *mengine_runtime_new(void) {
    MEngineRuntime *rt = malloc(sizeof(MEngineRuntime));
    if (!rt) {
        return NULL;
    }

    rt->ctx = context_create_empty();
    if (!rt->ctx) {
        free(rt);
        return NULL;
    }

    rt->def_table = malloc(sizeof(DefinitionTable));
    if (!rt->def_table) {
        free(rt);
        return NULL;
    }
    definition_table_init(rt->def_table);

    mengine_runtime_command_mode(rt);

    return rt;
}

void mengine_runtime_free(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    // TODO: A memory management strategy forcontexts and expressions is needed.
    // free_context(rt->ctx);
    definition_table_free(rt->def_table);
    free(rt);
}

Context *mengine_runtime_context(MEngineRuntime *rt) {
    if (!rt) {
        return NULL;
    }
    return rt->ctx;
}

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
        case CMD_DECL_KEYWORD:
        case CMD_STMT_KEYWORD:
        default:
            return;
    }
}

void mengine_runtime_proof_mode(MEngineRuntime *rt, Expression *theorem) {
    if (!rt || !theorem) {
        return;
    }

    rt->mode = MENGINE_RUNTIME_PROOF_MODE;
    rt->pending_theorem = theorem;
    debug_print_mode_update(rt);
}

void mengine_runtime_command_mode(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    rt->mode = MENGINE_RUNTIME_COMMAND_MODE;
    rt->pending_theorem = NULL;
    debug_print_mode_update(rt);
}

void mengine_repl(MEngineRuntime *rt) {
    if (!rt) return;

    char buffer[REPL_LINE_CAP];

    MEngineOptions options = {.debug = true,
                              .debug__print_tokens = true,
                              .debug__print_ast = true,
                              .debug__print_mode = true};

    printf("MEngine REPL. Type 'quit.' to exit.\n");
    printf("> ");
    fflush(stdout);

    while (fgets(buffer, REPL_LINE_CAP, stdin) != NULL) {
        char *input = buffer;
        while (*input == ' ' || *input == '\t' || *input == '\n' ||
               *input == '\r')
            input++;

        if (strncmp(input, "quit.", 5) == 0) break;

        if (*input == '\0') {
            printf("> ");
            fflush(stdout);
            continue;
        }

        Lexer lx;
        lexer_init(&lx, input, &options);

        Parser parser;
        parser_init(&parser, &lx, &options);

        Command *cmd = NULL;

        switch (rt->mode) {
            case (MENGINE_RUNTIME_COMMAND_MODE): {
                cmd = parse_command(&parser);
                if (!cmd) {
                    printf("Parse error.\n");
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                mengine_execute_command(rt, cmd);
                // todo: command freeing
                break;
            }
            case (MENGINE_RUNTIME_PROOF_MODE): {
                // ?
            }
        }

        printf("> ");
        fflush(stdout);
    }

    printf("Goodbye.\n");
}
