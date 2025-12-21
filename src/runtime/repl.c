#include "src/runtime/repl.h"

#include <stdio.h>

#include "src/common/color.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"
#include "src/runtime/proof_state.h"
#include "src/runtime/runtime.h"

void trim_whitespace(char *input) {
    while (*input == ' ' || *input == '\t' || *input == '\n' ||
           *input == '\r') {
        input++;
    }
}

void prompt() {
    printf("> ");
    fflush(stdout);
}

void print_prompt_and_goal(MEngineRuntime *rt) {
    prompt();

    if (rt->mode == MENGINE_RUNTIME_PROOF_MODE && rt->proof_state) {
        Expression *current_goal = proof_state_current(rt->proof_state);
        if (current_goal) {
            Context *goal_ctx = get_expression_context(current_goal);
            printf("\n" CYN "Context:" CRESET "\n%s\n",
                   stringify_context(goal_ctx));
            printf(CYN "Goal:" CRESET "\n%s\n",
                   stringify_expression(get_expression_type(current_goal)));
        }
    }

    fflush(stdout);
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
