#include "src/engine/engine_api.h"

#include "src/common/doubly_linked_list.h"
#include "src/engine/proof_state.h"
#include "src/engine/rewrite_internal.h"
#include "src/engine/tactic_api.h"
#include "src/engine/tactics.h"
#include "src/engine/unify.h"
#include "src/kernel/kernel_api.h"

/* PROOF STATE OPERATIONS */

ProofState *engine_proof_state_create(Expression *pending_theorem) {
    return proof_state_new(pending_theorem);
}

void engine_proof_state_free(ProofState *ps) { proof_state_free(ps); }

Expression *engine_proof_state_current_goal(ProofState *ps) { return proof_state_current(ps); }

Expression *engine_proof_state_pending_theorem(ProofState *ps) {
    return proof_state_pending_theorem(ps);
}

bool engine_proof_state_next_goal(ProofState *ps) { return proof_state_next(ps); }

void engine_proof_state_add_goals(ProofState *ps, void *new_goals_list) {
    proof_state_add_goals(ps, (DoublyLinkedList *)new_goals_list);
}

/* UNIFICATION OPERATIONS */

UnificationResult *engine_unify(Context *goal_context, Expression *lemma, Expression *goal) {
    return unify_and_instantiate(goal_context, lemma, kernel_expr_type(lemma), goal);
}

UnificationResult *engine_eunify(Expression *lemma, Expression *goal) {
    return eunify2(lemma, goal);
}

Expression *engine_unify_get_lemma(UnificationResult *unif_result) {
    return unification_result_get_lemma_instantiation(unif_result);
}

void *engine_unify_get_bindings(UnificationResult *unif_result) {
    return (void *)unification_result_get_new_goals(unif_result);
}

void engine_unify_free(UnificationResult *unif_result) { free_unification_result(unif_result); }

/* TACTIC OPERATIONS */

TacticResult *engine_tactic_intro(Expression *goal, char *name) { return intro_tactic(goal, name); }

TacticResult *engine_tactic_intros(Expression *goal, char **names, size_t name_count) {
    return intros_tactic(goal, names, name_count);
}

TacticResult *engine_tactic_apply(Expression *goal, Expression *lemma) {
    return apply_tactic(goal, lemma);
}

TacticResult *engine_tactic_eapply(Expression *goal, Expression *lemma) {
    return eapply_tactic(goal, lemma);
}

TacticResult *engine_tactic_exact(Expression *goal, Expression *proof_term) {
    return exact_tactic(goal, proof_term);
}

TacticResult *engine_tactic_assumption(Expression *goal) { return assumption_tactic(goal); }

TacticResult *engine_tactic_reflexivity(Expression *goal) {
    (void)goal; /* stub for now - reflexivity isn't implemented yet */
    return init_tactic_result(false, NULL, "reflexivity not yet implemented");
}

TacticResult *engine_tactic_split(Expression *goal) {
    (void)goal; /* stub for now */
    return init_tactic_result(false, NULL, "split not yet implemented");
}

TacticResult *engine_tactic_left(Expression *goal) {
    (void)goal; /* stub for now */
    return init_tactic_result(false, NULL, "left not yet implemented");
}

TacticResult *engine_tactic_right(Expression *goal) {
    (void)goal;
    return init_tactic_result(false, NULL, "right not yet implemented");
}

TacticResult *engine_tactic_exists(Expression *goal, Expression *witness) {
    (void)goal;
    (void)witness; /* stub for now */
    return init_tactic_result(false, NULL, "exists not yet implemented");
}

TacticResult *engine_tactic_cbv(Expression *goal, char **rules, int rule_count) {
    return cbv_tactic(goal, rules, rule_count);
}

bool engine_tactic_result_success(TacticResult *result) {
    return tactic_result_get_success(result);
}

char *engine_tactic_result_error(TacticResult *result) {
    return tactic_result_get_error_message(result);
}

void *engine_tactic_result_goals(TacticResult *result) {
    return (void *)tactic_result_get_goals(result);
}

void engine_tactic_result_free(TacticResult *result) { free_tactic_result(result); }

TacticResult *engine_tactic_rewrite(Expression *goal, Expression *lemma) {
    return rewrite_tactic(goal, lemma);
}

TacticResult *engine_tactic_erewrite(Expression *goal, Expression *lemma) {
    return erewrite_tactic(goal, lemma);
}

/* REWRITING OPERATIONS */

RewriteResult *engine_rewrite(Expression *expr, Expression *lemma, Context *context) {
    return rewrite(expr, lemma, context);
}

RewriteResult *engine_erewrite(Expression *expr, Expression *lemma, Context *context) {
    return erewrite(expr, lemma, context);
}

Expression *engine_rewrite_result_original(RewriteResult *result) {
    return rewrite_result_get_original(result);
}

Expression *engine_rewrite_result_rewritten(RewriteResult *result) {
    return rewrite_result_get_rewritten(result);
}

void *engine_rewrite_result_goals(RewriteResult *result) {
    return (void *)rewrite_result_get_new_goals(result);
}

void engine_rewrite_result_free(RewriteResult *result) { free_rewrite_result(result); }
