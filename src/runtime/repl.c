#include "src/runtime/repl.h"

#include <stdio.h>
#include <stdlib.h>

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
    printf("> ");
    fflush(stdout);
}

void prompt_proof_mode(MEngineRuntime *rt) {
    if (!rt || !rt->proof_state) {
        return;
    }

    Expression *current_goal = proof_state_current(rt->proof_state);
    Context *runtime_ctx = mengine_runtime_context(rt);
    if (!current_goal || !runtime_ctx) {
        return;
    }

    Context *goal_ctx = get_expression_context(current_goal);
    char *ctx_str = NULL;
    ctx_str = stringify_context_until(goal_ctx, runtime_ctx);
    printf("\n" CYN "Context:" CRESET "\n%s\n", ctx_str);
    free(ctx_str);
    printf(CYN "Goal:" CRESET "\n%s\n", stringify_expression(get_expression_type(current_goal)));
    prompt();
}

void print_prompt_and_goal(MEngineRuntime *rt) {
    if (rt->mode == MENGINE_RUNTIME_PROOF_MODE) {
        prompt_proof_mode(rt);
    } else {
        prompt();
    }
}

void mengine_repl(MEngineRuntime *rt) {
    if (!rt) {
        return;
    }

    char buffer[REPL_LINE_CAP];

    printf("MEngine REPL. Type 'quit.' to exit.\n");
    printf("> ");
    fflush(stdout);

    while (fgets(buffer, REPL_LINE_CAP, stdin) != NULL) {
        trim_whitespace(buffer);
        if (strncmp(buffer, "quit.", 5) == 0) {
            break;
        }

        if (*buffer == '\0') {
            prompt();
            continue;
        }

        int rc = mengine_runtime_exec_string(rt, buffer);
        if (rc != 0) {
            printf("Error in command.\n");
        }

        print_prompt_and_goal(rt);
    }

    printf("Goodbye.\n");
}
