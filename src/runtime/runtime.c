#include "src/runtime/runtime.h"

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

static void _handle_definition_command(MEngineRuntime *rt,
                                       DefinitionCmd *defn_cmd) {
    if (!rt || !defn_cmd) return;

    const char *name = defn_cmd->name;

    Context *ctx = rt->ctx;

    // Lefthand side parameters
    Binder **params = defn_cmd->params;
    size_t param_count = defn_cmd->param_count;

    for (size_t i = 0; i < param_count; i++) {
        Binder *b = params[i];

        Expression *param_type = ast_to_expression(b->type, ctx);
        if (!param_type) {
            fprintf(stderr, "Failed to convert parameter %s type.\n", b->name);
            return;
        }

        Expression *param_var =
            init_var_expression_wc(b->name, param_type, ctx);

        ctx = context_insert(ctx, param_var);
    }

    Expression *expr_type = ast_to_expression(defn_cmd->type, ctx);
    if (!expr_type) {
        fprintf(stderr, "Failed to convert type for Definition %s\n", name);
        return;
    }

    Expression *expr_body = ast_to_expression(defn_cmd->body, ctx);
    if (!expr_body) {
        fprintf(stderr, "Failed to convert body for Definition %s\n", name);
        return;
    }

    Expression *body_ty = get_expression_type(expr_body);

    if (!congruence(body_ty, expr_type)) {
        fprintf(stderr,
                RED "Type error:" CRESET
                    " body of definition '%s' has wrong type.\n",
                name);
        fprintf(stderr, "Declared type: %s\n", stringify_expression(expr_type));
        fprintf(stderr, "Body type:     %s\n", stringify_expression(body_ty));
        return;
    }

    // Store the definition in the definition table
    definition_table_insert(rt->def_table, name, expr_type, expr_body);

    printf("%s %s : %s defined.\n", "Definition", name,
           stringify_expression(expr_type));
}

static void _handle_statement_command(MEngineRuntime *rt,
                                      StatementCmd *stmt_cmd) {
    printf("not implemented yet\n");
    return;
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
        case CMD_DECL_KEYWORD:
        case CMD_STMT_KEYWORD:
        default:
            return;
    }
}

void mengine_repl(MEngineRuntime *rt) {
    if (!rt) return;

    char buffer[REPL_LINE_CAP];

    MEngineOptions options = {
        .debug = true, .debug__print_tokens = true, .debug__print_ast = true};

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

        cmd = parse_command(&parser);

        if (!cmd) {
            printf("Parse error.\n");
            printf("> ");
            fflush(stdout);
            continue;
        }

        mengine_execute_command(rt, cmd);

        // todo: command freeing

        printf("> ");
        fflush(stdout);
    }

    printf("Goodbye.\n");
}
