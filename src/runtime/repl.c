#include "src/runtime/repl.h"

void mengine_repl(MEngineRuntime *rt) {
    if (!rt) return;

    char buffer[REPL_LINE_CAP];

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
        lexer_init(&lx, input, rt->options);

        Parser parser;
        parser_init(&parser, &lx, rt->options);

        switch (rt->mode) {
            case (MENGINE_RUNTIME_COMMAND_MODE): {
                Command *cmd = command_parse_command(&parser);
                if (!cmd) {
                    printf("Command parse error.\n");
                    printf("> ");
                    fflush(stdout);
                    continue;
                }

                mengine_execute_command(rt, cmd);
                // todo: command freeing
                break;
            }
            case (MENGINE_RUNTIME_PROOF_MODE): {
                Tactic *tactic = tactic_parse_proof_command(&parser);
                if (!tactic) {
                    printf("Tactic parse error.\n");
                    printf("> ");
                    fflush(stdout);
                    continue;
                }
            }
        }

        printf("> ");
        fflush(stdout);
    }

    printf("Goodbye.\n");
}
