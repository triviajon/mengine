#include "src/tacticlanguage/tactic_exec.h"
#include "src/engine/new_tactics.h"
#include "src/metalanguage/ast_to_expression.h"
#include "src/runtime/proof_state.h"
#include "src/common/color.h"
#include "src/kernel/utils.h"

static Expression *_current_goal(MEngineRuntime *rt) {
    return proof_state_current(rt->proof_state);
}

static void _handle_proof_tactic(MEngineRuntime *rt) {
    // nothing to do for now!
    (void)rt;
}

static void _handle_qed_tactic(MEngineRuntime *rt) {
    Expression *thm = rt->pending_theorem;
    rt->ctx = context_insert(rt->ctx, thm);
    mengine_runtime_command_mode(rt);
}

static void _handle_admitted_tactic(MEngineRuntime *rt) {
    Expression *thm = rt->pending_theorem;
    rt->ctx = context_insert(rt->ctx, thm);
    mengine_runtime_command_mode(rt);
}

static void _handle_intro_tactic(MEngineRuntime *rt, IntroTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = intro_tactic(g, t->name);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, "Error: %s\n", result->error_message);
    }
    free_tactic_result(result);
    (void)t;
}

static void _handle_intros_tactic(MEngineRuntime *rt, IntrosTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = intros_tactic(g, t->names, t->name_count);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, "Error: %s\n", result->error_message);
    }
    free_tactic_result(result);
}

static void _handle_apply_tactic(MEngineRuntime *rt, ApplyTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, "Error: could not resolve lemma\n");
        return;
    }

    TacticResult *result = apply_tactic(goal, lemma);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, "Error: %s\n", result->error_message);
    }
    free_tactic_result(result);
}

static void _handle_eapply_tactic(MEngineRuntime *rt, EapplyTactic *t) {
    (void)rt;
    (void)t;
}

static void _handle_exact_tactic(MEngineRuntime *rt, ExactTactic *t) {
    (void)rt;
    (void)t;
}

static void _handle_rewrite_tactic(MEngineRuntime *rt, RewriteTactic *t) {
    Expression *g = _current_goal(rt);
    (void)t;
    (void)g;
}

static void _handle_reflexivity_tactic(MEngineRuntime *rt) { (void)rt; }

static void _handle_assumption_tactic(MEngineRuntime *rt) { (void)rt; }

static void _handle_split_tactic(MEngineRuntime *rt) { (void)rt; }

static void _handle_left_tactic(MEngineRuntime *rt) { (void)rt; }

static void _handle_right_tactic(MEngineRuntime *rt) { (void)rt; }

static void _handle_exists_tactic(MEngineRuntime *rt, ExistsTactic *t) {
    (void)rt;
    (void)t;
}

void _mengine_dispatch_tactic(MEngineRuntime *rt, Tactic *tac) {
    switch (tac->tag) {
        case TACTIC_PROOF:
            return _handle_proof_tactic(rt);

        case TACTIC_QED:
            return _handle_qed_tactic(rt);

        case TACTIC_ADMITTED:
            return _handle_admitted_tactic(rt);

        case TACTIC_INTRO:
            return _handle_intro_tactic(rt, &tac->as.intro);

        case TACTIC_INTROS:
            return _handle_intros_tactic(rt, &tac->as.intros);

        case TACTIC_APPLY:
            return _handle_apply_tactic(rt, &tac->as.apply);

        case TACTIC_EAPPLY:
            return _handle_eapply_tactic(rt, &tac->as.eapply);

        case TACTIC_EXACT:
            return _handle_exact_tactic(rt, &tac->as.exact);

        case TACTIC_REWRITE:
            return _handle_rewrite_tactic(rt, &tac->as.rewrite);

        case TACTIC_REFLEXIVITY:
            return _handle_reflexivity_tactic(rt);

        case TACTIC_ASSUMPTION:
            return _handle_assumption_tactic(rt);

        case TACTIC_SPLIT:
            return _handle_split_tactic(rt);

        case TACTIC_LEFT:
            return _handle_left_tactic(rt);

        case TACTIC_RIGHT:
            return _handle_right_tactic(rt);

        case TACTIC_EXISTS:
            return _handle_exists_tactic(rt, &tac->as.exists);
    }
}

void mengine_execute_tactic(MEngineRuntime *rt, Tactic *tac) {
    if (!rt || !rt->proof_state || !tac) return;

    _mengine_dispatch_tactic(rt, tac);

    // Each tactic handler will take care of adding new goals. We just need to
    // advance to the next goal if there is one.
    if (!proof_state_next(rt->proof_state)) {
        // No more goals - proof is complete!
        Expression *thm = rt->pending_theorem;
        rt->ctx = context_insert(rt->ctx, thm);

        printf(GRN "Proof complete." CRESET " %s declared.\n",
               thm->value.var.name);

        mengine_runtime_command_mode(rt);
    }
}
