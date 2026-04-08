#include "src/tacticlanguage/tactic_exec.h"

#include "src/common/color.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_parser.h"
#include "src/termlanguage/ast_to_expression.h"

static Expression *_current_goal(MEngineRuntime *rt) {
    return engine_proof_state_current_goal(rt->proof_state);
}

static bool _handle_admitted_tactic(MEngineRuntime *rt) {
    Expression *thm = rt->pending_theorem;
    rt->ctx = thm;
    mengine_runtime_command_mode(rt);
    return true;
}

static bool _handle_intro_tactic(MEngineRuntime *rt, IntroTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = engine_tactic_intro(g, t->name);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_intros_tactic(MEngineRuntime *rt, IntrosTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = engine_tactic_intros(g, t->names, t->name_count);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_apply_tactic(MEngineRuntime *rt, ApplyTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve lemma\n" CRESET);
        return false;
    }

    TacticResult *result = engine_tactic_apply(goal, lemma);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_eapply_tactic(MEngineRuntime *rt, EapplyTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

    Expression *lemma = ast_to_expression(t->lemma, ctx);
    if (!lemma) {
        fprintf(stderr, ERROR "Could not resolve lemma\n" CRESET);
        return false;
    }

    TacticResult *result = engine_tactic_eapply(goal, lemma);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_exact_tactic(MEngineRuntime *rt, ExactTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

    Expression *proof_term = ast_to_expression(t->proof_term, ctx);
    if (!proof_term) {
        fprintf(stderr, ERROR "Could not resolve proof term\n" CRESET);
        return false;
    }

    TacticResult *result = engine_tactic_exact(goal, proof_term);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_rewrite_tactic(MEngineRuntime *rt, RewriteTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

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

    TacticResult *result = engine_tactic_rewrite(goal, lemma);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_erewrite_tactic(MEngineRuntime *rt, RewriteTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

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

    TacticResult *result = engine_tactic_erewrite(goal, lemma);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_reflexivity_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = engine_tactic_reflexivity(goal);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_assumption_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = engine_tactic_assumption(goal);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_split_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = engine_tactic_split(goal);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_left_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = engine_tactic_left(goal);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_right_tactic(MEngineRuntime *rt) {
    Expression *goal = _current_goal(rt);
    TacticResult *result = engine_tactic_right(goal);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_exists_tactic(MEngineRuntime *rt, ExistsTactic *t) {
    Expression *goal = _current_goal(rt);
    Context *ctx = kernel_expr_context(goal);

    Expression *witness = ast_to_expression(t->witness, ctx);
    if (!witness) {
        fprintf(stderr, ERROR "Could not resolve witness\n" CRESET);
        return false;
    }

    TacticResult *result = engine_tactic_exists(goal, witness);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
}

static bool _handle_cbv_tactic(MEngineRuntime *rt, CbvTactic *t) {
    Expression *g = _current_goal(rt);
    TacticResult *result = engine_tactic_cbv(g, t->rules, t->rules_count);
    if (engine_tactic_result_success(result)) {
        engine_proof_state_add_goals(rt->proof_state, engine_tactic_result_goals(result));
    } else {
        fprintf(stderr, ERROR "%s\n" CRESET, engine_tactic_result_error(result));
    }
    bool success = engine_tactic_result_success(result);
    engine_tactic_result_free(result);
    return success;
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

        case TACTIC_CBV:
            return _handle_cbv_tactic(rt, &tac->as.cbv);
    }
    return false;
}

int mengine_execute_tactic(MEngineRuntime *rt, TacticExpr *tac) {
    if (!rt || !rt->proof_state || !tac) {
        return 1;
    }

    // For now, only handle primitive tactics (will be replaced by interpreter)
    if (tac->tag != TAC_PRIMITIVE) {
        fprintf(stderr, ERROR "Non-primitive tactic expressions not yet supported\n" CRESET);
        return 1;
    }

    Tactic *prim = tac->as.primitive.tactic;
    bool success = _mengine_dispatch_tactic(rt, prim);

    // Only proceed if the tactic succeeded
    if (!success) {
        return 1;
    }

    if (prim->tag == TACTIC_ADMITTED) {
        return 0;
    }

    // Each tactic handler will take care of adding new goals. We just need to
    // advance to the next goal if there is one.
    if (!engine_proof_state_next_goal(rt->proof_state)) {
        // No more goals - proof is complete!
        Expression *thm = rt->pending_theorem;
        rt->ctx = thm;

        MPRINT(rt->options->quiet, stdout, SUCCESS "Proof complete." CRESET " %s declared.\n",
               kernel_var_name(thm));

        mengine_runtime_command_mode(rt);
    }
    return 0;
}
