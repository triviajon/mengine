#include "src/runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/commandlanguage/command_exec.h"
#include "src/common/color.h"
#include "src/common/options.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"
#include "src/runtime/core.h"
#include "src/runtime/proof_state.h"
#include "src/tacticlanguage/tactic_exec.h"

void debug_print_mode(MEngineRuntime *rt) {
    if (!rt || !rt->options) {
        return;
    }
    if (!rt->options->debug || !rt->options->debug__print_mode) {
        return;
    }

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
            // todo: I also should print proof_state
            break;

        default:
            fprintf(stderr, "UNKNOWN_MODE");
            break;
    }

    fprintf(stderr, CRESET "\n");
}

MEngineRuntime *mengine_runtime_new(MEngineOptions *options) {
    MEngineRuntime *rt = malloc(sizeof(MEngineRuntime));
    if (!rt) {
        return NULL;
    }

    rt->options = options;

    rt->ctx = context_create_empty();
    if (!rt->ctx) {
        free(rt);
        return NULL;
    }

    init_core(&rt->ctx);

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

int mengine_runtime_exec_string(MEngineRuntime *rt, const char *source) {
    if (!rt || !source) {
        return 1;
    }

    Lexer lx;
    lexer_init(&lx, source, rt->options);

    Parser parser;
    parser_init(&parser, &lx, rt->options);

    int rc = 0;

    parser.error_recovery_set = true;

    while (!parser_eof(&parser)) {
        if (setjmp(parser.error_jmp) != 0) {
            // Parse error occurred
            rc = 1;
            break;
        }

        switch (rt->mode) {
            case MENGINE_RUNTIME_COMMAND_MODE: {
                Command *cmd = command_parse_command(&parser);
                if (!cmd) {
                    fprintf(stderr, "Command parse error.\n");
                    rc = 1;
                    break;
                }
                mengine_execute_command(rt, cmd);
                // TODO: free Command
                break;
            }

            case MENGINE_RUNTIME_PROOF_MODE: {
                Tactic *tactic = tactic_parse_proof_command(&parser);
                if (!tactic) {
                    fprintf(stderr, "Tactic parse error.\n");
                    rc = 1;
                    break;
                }
                mengine_execute_tactic(rt, tactic);
                // TODO: free Tactic
                break;
            }
        }

        if (rc != 0) {
            break;
        }
    }

    return rc;
}

int mengine_runtime_exec_file(MEngineRuntime *rt, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, "Out of memory reading file: %s\n", filename);
        return 1;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    int rc = mengine_runtime_exec_string(rt, buf);
    free(buf);
    return rc;
}

Context *mengine_runtime_context(MEngineRuntime *rt) {
    if (!rt) {
        return NULL;
    }
    return rt->ctx;
}

void mengine_runtime_proof_mode(MEngineRuntime *rt, Expression *theorem) {
    if (!rt || !theorem) {
        return;
    }

    rt->mode = MENGINE_RUNTIME_PROOF_MODE;
    rt->pending_theorem = theorem;
    rt->proof_state = proof_state_new_from_theorem(theorem, rt->ctx);
    debug_print_mode(rt);
}

void mengine_runtime_command_mode(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    rt->mode = MENGINE_RUNTIME_COMMAND_MODE;
    rt->pending_theorem = NULL;

    if (rt->proof_state) {
        proof_state_free(rt->proof_state);
        rt->proof_state = NULL;
    }

    debug_print_mode(rt);
}