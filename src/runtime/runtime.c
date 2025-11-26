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

    return rt;
}

void mengine_runtime_free(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    // TODO: A memory management strategy forcontexts and expressions is needed.
    // For now, we just free the runtime struct itself.
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
    printf("not implemented yet\n");
    return;
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
