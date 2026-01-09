#include "src/tacticlanguage/tactic_exec.h"

#include "src/common/color.h"
#include "src/engine/tactics.h"
#include "src/runtime/proof_state.h"
#include "src/termlanguage/ast_to_expression.h"

static Expression *_current_goal(MEngineRuntime *rt) {
    return proof_state_current(rt->proof_state);
}

static bool _handle_admitted_tactic(MEngineRuntime *rt) {
    Expression *thm = rt->pending_theorem;
    rt->ctx = thm;
    mengine_runtime_command_mode(rt);
    return true;
}

static bool _handle_intro_tactic(MEngineRuntime *rt, IntroTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = intro_tactic(g, t->name);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    (void)t;
    return success;
}

static bool _handle_intros_tactic(MEngineRuntime *rt, IntrosTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = intros_tactic(g, t->names, t->name_count);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_apply_tactic(MEngineRuntime *rt, ApplyTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve lemma\n" CRESET);
        return false;
    }

    TacticResult *result = apply_tactic(goal, lemma);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_eapply_tactic(MEngineRuntime *rt, EapplyTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve lemma\n" CRESET);
        return false;
    }

    TacticResult *result = eapply_tactic(goal, lemma);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_exact_tactic(MEngineRuntime *rt, ExactTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *proof_term = ast_to_expression(t->proof_term, ctx);
    if (!proof_term) {
        fprintf(stderr, ERROR "Could not resolve proof term\n" CRESET);
        return false;
    }

    TacticResult *result = exact_tactic(goal, proof_term);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_rewrite_tactic(MEngineRuntime *rt, RewriteTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve rewrite lemma\n" CRESET);
        return false;
    }

    Expression *equiv_proof = ast_to_expression(t->equiv_proof, ctx);
    if (!equiv_proof) {
        fprintf(stderr, ERROR "Could not resolve equivalence proof\n" CRESET);
        return false;
    }

    // we're not actually handling the backward flag right now... but we should handle it with an
    // application of the equivalence symmetry
    TacticResult *result = rewrite_tactic(goal, lemma);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_erewrite_tactic(MEngineRuntime *rt, RewriteTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = get_expression_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve rewrite lemma\n" CRESET);
        return false;
    }

    Expression *equiv_proof = ast_to_expression(t->equiv_proof, ctx);
    if (!equiv_proof) {
        fprintf(stderr, ERROR "Could not resolve equivalence proof\n" CRESET);
        return false;
    }

    // we're not actually handling the backward flag right now... but we should handle it with an
    // application of the equivalence symmetry
    TacticResult *result = erewrite_tactic(goal, lemma);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_reflexivity_tactic(MEngineRuntime *rt) {
    (void)rt;
    return true;
}

static bool _handle_assumption_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = assumption_tactic(goal);
    if (result->success) {
        proof_state_add_goals(rt->proof_state, result->new_goals);
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, result->error_message);
    }
    bool success = result->success;
    free_tactic_result(result);
    return success;
}

static bool _handle_split_tactic(MEngineRuntime *rt) {
    (void)rt;
    return true;
}

static bool _handle_left_tactic(MEngineRuntime *rt) {
    (void)rt;
    return true;
}

static bool _handle_right_tactic(MEngineRuntime *rt) {
    (void)rt;
    return true;
}

static bool _handle_exists_tactic(MEngineRuntime *rt, ExistsTactic *t) {
    (void)rt;
    (void)t;
    return true;
}

bool _mengine_dispatch_tactic(MEngineRuntime *rt, Tactic *tac) {
    switch (tac->tag) {
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
        case TACTIC_REWRITE_BACKWARD:
            return _handle_rewrite_tactic(rt, &tac->as.rewrite);

        case TACTIC_EREWRITE:
        case TACTIC_EREWRITE_BACKWARD:
            return _handle_erewrite_tactic(rt, &tac->as.rewrite);

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
    return false;
}

int mengine_execute_tactic(MEngineRuntime *rt, Tactic *tac) {
    if (!rt || !rt->proof_state || !tac) {
        return 1;
    }

    bool success = _mengine_dispatch_tactic(rt, tac);

    // Only proceed if the tactic succeeded
    if (!success) {
        return 1;
    }

    if (tac->tag == TACTIC_ADMITTED) {
        return 0;
    }

    // Each tactic handler will take care of adding new goals. We just need to
    // advance to the next goal if there is one.
    if (!proof_state_next(rt->proof_state)) {
        // No more goals - proof is complete!
        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;

        MPRINT(rt->options->quiet, stdout, SUCCESS "Proof complete." CRESET " %s declared.\n",
               get_var_name(thm));

        mengine_runtime_command_mode(rt);
    }
    return 0;
}
