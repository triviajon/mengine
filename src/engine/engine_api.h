#ifndef ENGINE_API_H
#define ENGINE_API_H

// engine_api.h - proof engine operations

#include <stdbool.h>
#include <stddef.h>

#include "src/kernel/kernel_api.h"

// Opaque engine types
typedef struct ProofState ProofState;
typedef struct TacticResult TacticResult;
typedef struct UnificationResult UnificationResult;
typedef struct RewriteResult RewriteResult;

/* ============================================================================
 * Proof State Operations
 * ============================================================================ */

ProofState *engine_proof_state_create(Expression *pending_theorem);

void engine_proof_state_free(ProofState *ps);

Expression *engine_proof_state_current_goal(ProofState *ps);

Expression *engine_proof_state_pending_theorem(ProofState *ps);

bool engine_proof_state_next_goal(ProofState *ps);

void engine_proof_state_add_goals(ProofState *ps, void *new_goals_list);

/* ============================================================================
 * Unification Operations
 * ============================================================================ */

UnificationResult *engine_unify(Context *goal_context, Expression *lemma, Expression *goal);

UnificationResult *engine_eunify(Expression *lemma, Expression *goal);

Expression *engine_unify_get_lemma(UnificationResult *unif_result);

void *engine_unify_get_bindings(UnificationResult *unif_result);

void engine_unify_free(UnificationResult *unif_result);

/* ============================================================================
 * Tactic Operations
 * ============================================================================ */

TacticResult *engine_tactic_intro(Expression *goal, char *name);

TacticResult *engine_tactic_intros(Expression *goal, char **names, size_t name_count);

TacticResult *engine_tactic_apply(Expression *goal, Expression *lemma);

TacticResult *engine_tactic_eapply(Expression *goal, Expression *lemma);

TacticResult *engine_tactic_exact(Expression *goal, Expression *proof_term);

TacticResult *engine_tactic_assumption(Expression *goal);

TacticResult *engine_tactic_reflexivity(Expression *goal);

TacticResult *engine_tactic_split(Expression *goal);

TacticResult *engine_tactic_left(Expression *goal);

TacticResult *engine_tactic_right(Expression *goal);

TacticResult *engine_tactic_exists(Expression *goal, Expression *witness);

TacticResult *engine_tactic_cbv(Expression *goal, char **rules, int rule_count);

TacticResult *engine_tactic_rewrite(Expression *goal, Expression *lemma);

TacticResult *engine_tactic_erewrite(Expression *goal, Expression *lemma);

bool engine_tactic_result_success(TacticResult *result);

char *engine_tactic_result_error(TacticResult *result);

void *engine_tactic_result_goals(TacticResult *result);

void engine_tactic_result_free(TacticResult *result);

/* ============================================================================
 * Rewriting Operations
 * ============================================================================ */

RewriteResult *engine_rewrite(Expression *expr, Expression *lemma, Context *context);

RewriteResult *engine_erewrite(Expression *expr, Expression *lemma, Context *context);

Expression *engine_rewrite_result_original(RewriteResult *result);

Expression *engine_rewrite_result_rewritten(RewriteResult *result);

void *engine_rewrite_result_goals(RewriteResult *result);

void engine_rewrite_result_free(RewriteResult *result);

#endif  // ENGINE_API_H
