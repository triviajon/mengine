#include "src/engine/engine_api.h"

#include "src/common/doubly_linked_list.h"
#include "src/common/timing.h"
#include "src/engine/proof_state.h"
#include "src/engine/rewrite_internal.h"
#include "src/engine/tactic_api.h"
#include "src/engine/tactics.h"
#include "src/engine/unify.h"
#include "src/kernel/kernel_api.h"

/* PROOF STATE OPERATIONS */

RelationRegistry *engine_relation_registry_create(void) { return relation_registry_new(); }

void engine_relation_registry_free(RelationRegistry *reg) { relation_registry_free(reg); }

bool engine_relation_registry_add(RelationRegistry *reg, EngineRelationInfo info) {
    RelationInfo internal = {
        .relation = info.relation,
        .refl = info.refl,
        .trans = info.trans,
        .congr = info.congr,
    };
    return relation_registry_add(reg, internal);
}

void engine_rewrite_set_debug(bool enabled) { rewrite_set_debug(enabled); }

void engine_rewrite_print_cumulative_stats(void) { rewrite_print_cumulative_stats(); }

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
    timer_push(TIMER_ENGINE);
    UnificationResult *r = unify_and_instantiate(goal_context, lemma, kernel_expr_type(lemma), goal);
    timer_pop();
    return r;
}

UnificationResult *engine_eunify(Expression *lemma, Expression *goal) {
    timer_push(TIMER_ENGINE);
    UnificationResult *r = eunify2(lemma, goal);
    timer_pop();
    return r;
}

UnificationResult *engine_rewrite_unify_for_eq(Context *goal_context, Expression *lemma,
                                               Expression *target) {
    timer_push(TIMER_ENGINE);
    UnificationResult *r = bad_unify_for_eq(goal_context, lemma, target);
    timer_pop();
    return r;
}

Expression *engine_unify_get_lemma(UnificationResult *unif_result) {
    return unification_result_get_lemma_instantiation(unif_result);
}

void *engine_unify_get_bindings(UnificationResult *unif_result) {
    return (void *)unification_result_get_new_goals(unif_result);
}

void *engine_unify_take_bindings(UnificationResult *unif_result) {
    DoublyLinkedList *goals = unification_result_get_new_goals(unif_result);
    if (unif_result) {
        unif_result->new_goals = NULL;
    }
    return goals;
}

void engine_unify_free(UnificationResult *unif_result) { free_unification_result(unif_result); }

/* TACTIC OPERATIONS */

TacticResult *engine_tactic_intro(Expression *goal, char *name) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = intro_tactic(goal, name);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_intros(Expression *goal, char **names, size_t name_count) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = intros_tactic(goal, names, name_count);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_apply(Expression *goal, Expression *lemma) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = apply_tactic(goal, lemma);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_eapply(Expression *goal, Expression *lemma) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = eapply_tactic(goal, lemma);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_exact(Expression *goal, Expression *proof_term) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = exact_tactic(goal, proof_term);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_assumption(Expression *goal) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = assumption_tactic(goal);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_reflexivity(Expression *goal) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = reflexivity_tactic(goal);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_split(Expression *goal) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = split_tactic(goal);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_left(Expression *goal) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = left_tactic(goal);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_right(Expression *goal) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = right_tactic(goal);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_exists(Expression *goal, Expression *witness) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = exists_tactic(goal, witness);
    timer_pop();
    return r;
}

TacticResult *engine_tactic_cbv(Expression *goal, char **rules, int rule_count) {
    timer_push(TIMER_ENGINE);
    TacticResult *r = cbv_tactic(goal, rules, rule_count);
    timer_pop();
    return r;
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

TacticResult *engine_tactic_result_new(bool success, void *new_goals, char *error_message) {
    return init_tactic_result(success, (DoublyLinkedList *)new_goals, error_message);
}

TacticResult *engine_tactic_result_value(TacticValue *value) {
    return init_tactic_result_value(value);
}

TacticValue *engine_tactic_result_get_value(TacticResult *result) {
    return tactic_result_get_value(result);
}

void *engine_tactic_result_take_goals(TacticResult *result) {
    DoublyLinkedList *goals = tactic_result_get_goals(result);
    if (result) {
        result->new_goals = NULL;
    }
    return goals;
}

TacticValue *engine_tactic_result_take_value(TacticResult *result) {
    TacticValue *value = tactic_result_get_value(result);
    if (result) {
        result->term_value = NULL;
    }
    return value;
}

void engine_tactic_result_set_goals(TacticResult *result, void *goals) {
    if (result) {
        result->new_goals = (DoublyLinkedList *)goals;
    }
}

void engine_tactic_result_set_value(TacticResult *result, TacticValue *value) {
    if (result) {
        result->term_value = value;
    }
}

TacticValue *engine_tactic_value_expr(Expression *expr) { return tactic_value_expr(expr); }

TacticValue *engine_tactic_value_pair(TacticValue *fst, TacticValue *snd) {
    return tactic_value_pair(fst, snd);
}

TacticValue *engine_tactic_value_ast(struct AST *ast) { return tactic_value_ast(ast); }

TacticValue *engine_tactic_value_dup(TacticValue *value) { return tactic_value_dup(value); }

void engine_tactic_value_free(TacticValue *value) { free_tactic_value(value); }

EngineTacticValueKind engine_tactic_value_kind(TacticValue *value) {
    if (!value) {
        return ENGINE_TVAL_EXPRESSION;
    }
    switch (value->kind) {
        case TVAL_EXPRESSION:
            return ENGINE_TVAL_EXPRESSION;
        case TVAL_PAIR:
            return ENGINE_TVAL_PAIR;
        case TVAL_AST:
            return ENGINE_TVAL_AST;
    }
    return ENGINE_TVAL_EXPRESSION;
}

Expression *engine_tactic_value_as_expr(TacticValue *value) { return tactic_value_as_expr(value); }

struct AST *engine_tactic_value_as_ast(TacticValue *value) {
    return value && value->kind == TVAL_AST ? value->ast : NULL;
}

TacticValue *engine_tactic_value_pair_fst(TacticValue *value) {
    return value && value->kind == TVAL_PAIR ? value->pair.fst : NULL;
}

TacticValue *engine_tactic_value_pair_snd(TacticValue *value) {
    return value && value->kind == TVAL_PAIR ? value->pair.snd : NULL;
}

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
