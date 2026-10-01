#include "src/tacticlanguage/tactic_exec.h"

#include <stdio.h>

#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/common/timing.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_interp.h"
#include "src/tacticlanguage/tactic_parser.h"

int mengine_execute_tactic(MEngineRuntime *rt, TacticExpr *tac) {
    if (!rt || !rt->proof_state || !tac) {
        return 1;
    }
    timer_push(TIMER_TACTIC);

    // Handle "Admitted" specially - it exits proof mode immediately
    if (tac->tag == TAC_PRIMITIVE && tac->as.primitive.tactic->tag == TACTIC_ADMITTED) {
        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;
        mengine_runtime_command_mode(rt);
        timer_pop();
        return 0;
    }

    // Get current goal and interpret the tactic expression
    Expression *goal = engine_proof_state_current_goal(rt->proof_state);
    if (!goal) {
        fprintf(stderr, ERROR "No current goal\n" CRESET);
        timer_pop();
        return 1;
    }

    TacticResult *result = tactic_interpret(rt, goal, tac);

    if (!engine_tactic_result_success(result)) {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
        engine_tactic_result_free(result);
        timer_pop();
        return 1;
    }

    DoublyLinkedList *new_goals = engine_tactic_result_take_goals(result);
    engine_tactic_result_free(result);

    if (kernel_hole_is_filled(goal)) {
        kernel_free_filled_hole(goal);
    } else if (kernel_expr_is_hole(goal) && (!new_goals || !dll_search(new_goals, goal))) {
        // The tactic succeeded without solving the goal, e.g. by filling another hole.
        if (!new_goals) {
            new_goals = dll_create();
        }
        dll_insert_at_head(new_goals, dll_new_node(goal));
    }

    // Add any new subgoals to the proof state
    engine_proof_state_add_goals(rt->proof_state, new_goals);

    // Advance to the next goal, skipping filled evar holes
    bool has_next = engine_proof_state_next_goal(rt->proof_state);
    Expression *next_active = engine_proof_state_current_goal(rt->proof_state);
    if (!has_next || !next_active) {
        // No more active goals - proof is complete!
        Expression *unfilled = engine_proof_state_unfilled_shelved(rt->proof_state);
        if (unfilled) {
            fprintf(stderr, ERROR "Proof incomplete: unresolved hole ?%s\n" CRESET,
                    kernel_hole_name(unfilled));
            timer_pop();
            return 1;
        }

        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;

        MPRINT(rt->options->quiet, stdout, SUCCESS "Proof complete." CRESET " %s declared.\n",
               kernel_var_name(thm));

        mengine_runtime_command_mode(rt);
    }

    timer_pop();
    return 0;
}
