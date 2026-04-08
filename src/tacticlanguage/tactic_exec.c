#include "src/tacticlanguage/tactic_exec.h"

#include <stdio.h>

#include "src/common/color.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_interp.h"
#include "src/tacticlanguage/tactic_parser.h"

int mengine_execute_tactic(MEngineRuntime *rt, TacticExpr *tac) {
    if (!rt || !rt->proof_state || !tac) {
        return 1;
    }

    // Handle "Admitted" specially — it exits proof mode immediately
    if (tac->tag == TAC_PRIMITIVE && tac->as.primitive.tactic->tag == TACTIC_ADMITTED) {
        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;
        mengine_runtime_command_mode(rt);
        return 0;
    }

    // Get current goal and interpret the tactic expression
    Expression *goal = engine_proof_state_current_goal(rt->proof_state);
    if (!goal) {
        fprintf(stderr, ERROR "No current goal\n" CRESET);
        return 1;
    }

    TacticResult *result = tactic_interpret(rt, goal, tac);

    if (!engine_tactic_result_success(result)) {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
        engine_tactic_result_free(result);
        return 1;
    }

    // Add any new subgoals to the proof state
    engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    engine_tactic_result_free(result);

    // Advance to the next goal
    if (!engine_proof_state_next_goal(rt->proof_state)) {
        // No more goals — proof is complete!
        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;

        MPRINT(rt->options->quiet, stdout, SUCCESS "Proof complete." CRESET " %s declared.\n",
               kernel_var_name(thm));

        mengine_runtime_command_mode(rt);
    }

    return 0;
}
