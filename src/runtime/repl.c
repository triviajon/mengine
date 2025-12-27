#include "src/runtime/repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"
#include "src/runtime/proof_state.h"
#include "src/runtime/runtime.h"

void trim_whitespace(char *s) {
    char *start = s;

    while (*start == ' ' || *start == '\t' || *start == '\n' ||
           *start == '\r') {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

void prompt() {
    fprintf(stdout, "> ");
    fflush(stdout);
}

static void prompt_normal(void) {
    fprintf(stdout, PROMPT "> " CRESET);
    fflush(stdout);
}

static void prompt_proof(void) {
    fprintf(stdout, PROMPT "proof ❯ " CRESET);
    fflush(stdout);
}

static void print_proof_state(MEngineRuntime *rt) {
    if (!rt || !rt->proof_state) {
        return;
    }

    Expression *goal = proof_state_current(rt->proof_state);
    Context *rt_ctx = mengine_runtime_context(rt);
    if (!goal || !rt_ctx) {
        return;
    }

    Context *goal_ctx = get_expression_context(goal);
    char *ctx_str =
        stringify_context_until(goal_ctx, rt_ctx, CTX_STRINGIFY_PRETTY_IND2);
    char *goal_str = stringify_expression(get_expression_type(goal));

    // Header
    Expression *current_theorem = rt->pending_theorem;
    char *theorem_name = get_var_name(current_theorem);
    fprintf(stdout, "\n" UI "⊢ " CRESET BOLD "%s\n", theorem_name);
    fprintf(stdout, UI "────────────────────────────────\n" CRESET);

    // Context
    if (ctx_str && *ctx_str) {
        fprintf(stdout, HEADER "Context:\n" CRESET);
        fprintf(stdout, CTXCLR "%s\n" CRESET, ctx_str);
    }

    // Goal
    fprintf(stdout, HEADER "Goal:\n" CRESET);
    fprintf(stdout, GOALCLR "  %s\n" CRESET, goal_str);

    free(ctx_str);
    free(goal_str);
}

void print_prompt_and_state(MEngineRuntime *rt) {
    if (rt->mode == MENGINE_RUNTIME_PROOF_MODE) {
        print_proof_state(rt);
        prompt_proof();
    } else {
        prompt_normal();
    }
}

void mengine_repl(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    char buffer[REPL_LINE_CAP];

    fprintf(stdout, UI "MEngine REPL. Type 'quit.' to exit.\n" CRESET);
    print_prompt_and_state(rt);

    while (fgets(buffer, REPL_LINE_CAP, stdin) != NULL) {
        trim_whitespace(buffer);

        // Handle graceful quitting
        if (strncmp(buffer, "quit.", 5) == 0) {
            break;
        }

        // If it's non-empty, handle the input
        if (*buffer != '\0') {
            int rc = mengine_runtime_exec_string(rt, buffer);
            if (rc != 0) {
                fprintf(stderr, ERROR "Error in command.\n" CRESET);
            }
        }

        print_prompt_and_state(rt);
        memset(buffer, 0, REPL_LINE_CAP);
    }

    fprintf(stdout, UI "Goodbye.\n" CRESET);
}
