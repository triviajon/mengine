#include "src/runtime/runtime.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/commandlanguage/command_exec.h"
#include "src/commandlanguage/command_parser.h"
#include "src/common/color.h"
#include "src/common/options.h"
#include "src/kernel/expression.h"
#include "src/kernel/inductive.h"
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
                fprintf(stderr, " (goal: %s)", stringify_expression(rt->pending_theorem));
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

    rt->proof_state = NULL;
    rt->pending_theorem = NULL;
    rt->options = options;

    rt->ctx = context_create_empty();
    if (!rt->ctx) {
        free(rt);
        return NULL;
    }

    // Initialize inductive type registry
    inductive_registry_init();

    init_core(&rt->ctx);
    mengine_runtime_command_mode(rt);

    return rt;
}

void mengine_runtime_free(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    // TODO: A memory management strategy...
    // free_context(rt->ctx);
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

    while (!parser_eof(&parser)) {
        if (setjmp(parser.error_jmp) != 0) {
            // Parse error occurred
            rc = 0;
            break;
        }

        switch (rt->mode) {
            case MENGINE_RUNTIME_COMMAND_MODE: {
                Command *cmd = command_parse_command(&parser);
                if (!cmd) {
                    fprintf(stderr, ERROR "Command parse error.\n" CRESET);
                    rc = 1;
                    break;
                }
                rc = mengine_execute_command(rt, cmd);
                if (rc != 0) {
                    fprintf(stderr, ERROR "Command execution error.\n" CRESET);
                    break;
                }
                parser_flush_comments(&parser);
                // TODO: free Command
                break;
            }

            case MENGINE_RUNTIME_PROOF_MODE: {
                // Check if the current token is a command keyword
                TokenType tok_type = parser.current->type;
                CommandDispatchEntry *cmd_entry = NULL;
                CMD_LOOKUP_ENTRY(&cmd_entry, tok_type);

                if (cmd_entry != NULL) {
                    Command *cmd = command_parse_command(&parser);
                    if (!cmd) {
                        fprintf(stderr, ERROR "Command parse error.\n" CRESET);
                        rc = 1;
                        break;
                    }
                    rc = mengine_execute_command(rt, cmd);
                    if (rc != 0) {
                        fprintf(stderr, ERROR "Command execution error.\n" CRESET);
                        break;
                    }
                    // Print any comments that came after this command
                    parser_flush_comments(&parser);
                    // TODO: free Command
                    break;
                }

                // It's a tactic, parse and execute it
                Tactic *tactic = tactic_parse_proof_command(&parser);
                if (!tactic) {
                    fprintf(stderr, ERROR "Tactic parse error.\n" CRESET);
                    rc = 1;
                    break;
                }
                rc = mengine_execute_tactic(rt, tactic);
                if (rc != 0) {
                    fprintf(stderr, ERROR "Tactic execution error.\n" CRESET);
                    break;
                }
                parser_flush_comments(&parser);
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
    if (!rt) {
        fprintf(stderr, ERROR "Failed to initialize MEngine runtime.\n" CRESET);
        return 1;
    }

    rt->options->execution_type = MENGINE_EXECUTION_TYPE_FILE;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, ERROR "Could not open file: %s\n" CRESET, filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        fprintf(stderr, ERROR "Out of memory reading file: %s\n" CRESET, filename);
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

void mengine_runtime_proof_mode(MEngineRuntime *rt, Expression *pending_theorem) {
    if (!rt || !pending_theorem) {
        return;
    }

    rt->mode = MENGINE_RUNTIME_PROOF_MODE;
    rt->pending_theorem = pending_theorem;
    rt->proof_state = proof_state_new(pending_theorem);

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