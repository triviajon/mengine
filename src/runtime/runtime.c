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