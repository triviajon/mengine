#ifndef ENGINE_API_H
#define ENGINE_API_H

// engine_api.h - proof engine operations

#include <stdbool.h>
#include <stddef.h>

#include "src/kernel/kernel_api.h"

// Opaque engine types
typedef struct ProofState ProofState;
typedef struct TacticResult TacticResult;
typedef struct TacticValue TacticValue;
typedef struct UnificationResult UnificationResult;
typedef struct RewriteResult RewriteResult;
typedef struct RelationRegistry RelationRegistry;

struct AST;

typedef enum { ENGINE_TVAL_EXPRESSION, ENGINE_TVAL_PAIR, ENGINE_TVAL_AST } EngineTacticValueKind;

typedef struct {
    Expression *relation;
    Expression *refl;
    Expression *trans;
    Expression *congr;
} EngineRelationInfo;

/* ============================================================================
 * Relation Registry Operations
 * ============================================================================ */

RelationRegistry *engine_relation_registry_create(void);
void engine_relation_registry_free(RelationRegistry *reg);
bool engine_relation_registry_add(RelationRegistry *reg, EngineRelationInfo info);

void engine_rewrite_set_debug(bool enabled);
void engine_rewrite_print_cumulative_stats(void);

/* ============================================================================
 * Proof State Operations
 * ============================================================================ */

/**
 * Create a new proof state for a pending theorem.
 * Time Complexity:
 *
 * @param pending_theorem
 * @return
 */
ProofState *engine_proof_state_create(Expression *pending_theorem);

/**
 * Free a proof state.
 * Time Complexity:
 *
 * @param ps
 * @return
 */
void engine_proof_state_free(ProofState *ps);

/**
 * Get the current goal from a proof state.
 * Time Complexity:
 *
 * @param ps
 * @return
 */
Expression *engine_proof_state_current_goal(ProofState *ps);

/**
 * Get the pending theorem from a proof state.
 * Time Complexity:
 *
 * @param ps
 * @return
 */
Expression *engine_proof_state_pending_theorem(ProofState *ps);

/**
 * Advance to the next goal in a proof state.
 * Time Complexity:
 *
 * @param ps
 * @return
 */
bool engine_proof_state_next_goal(ProofState *ps);

/**
 * Add new goals to the proof state's goal list.
 * Time Complexity:
 *
 * @param ps
 * @param new_goals_list
 * @return
 */
void engine_proof_state_add_goals(ProofState *ps, void *new_goals_list);

/* ============================================================================
 * Unification Operations
 * ============================================================================ */

/**
 * Unify a lemma with a goal under a given context.
 * Time Complexity:
 *
 * @param goal_context
 * @param lemma
 * @param goal
 * @return
 */
UnificationResult *engine_unify(Context *goal_context, Expression *lemma, Expression *goal);

/**
 * Unify a lemma with a goal allowing existential instantiations.
 * Time Complexity:
 *
 * @param lemma
 * @param goal
 * @return
 */
UnificationResult *engine_eunify(Expression *lemma, Expression *goal);

/**
 * Unify an equality lemma's left-hand side against a target expression.
 * This is used by the tactic language's scripted rewrite combinator.
 */
UnificationResult *engine_rewrite_unify_for_eq(Context *goal_context, Expression *lemma,
                                               Expression *target);

/**
 * Get the instantiated lemma from a unification result.
 * Time Complexity:
 *
 * @param unif_result
 * @return
 */
Expression *engine_unify_get_lemma(UnificationResult *unif_result);

/**
 * Get the bindings from a unification result.
 * Time Complexity:
 *
 * @param unif_result
 * @return
 */
void *engine_unify_get_bindings(UnificationResult *unif_result);

/**
 * Transfer ownership of open goals from a unification result.
 */
void *engine_unify_take_bindings(UnificationResult *unif_result);

/**
 * Free a unification result.
 * Time Complexity:
 *
 * @param unif_result
 * @return
 */
void engine_unify_free(UnificationResult *unif_result);

/* ============================================================================
 * Tactic Operations
 * ============================================================================ */

/**
 * Apply the intro tactic to a goal.
 * Time Complexity:
 *
 * @param goal
 * @param name
 * @return
 */
TacticResult *engine_tactic_intro(Expression *goal, char *name);

/**
 * Apply the intros tactic to a goal.
 * Time Complexity:
 *
 * @param goal
 * @param names
 * @param name_count
 * @return
 */
TacticResult *engine_tactic_intros(Expression *goal, char **names, size_t name_count);

/**
 * Apply the apply tactic to a goal using a lemma.
 * Time Complexity:
 *
 * @param goal
 * @param lemma
 * @return
 */
TacticResult *engine_tactic_apply(Expression *goal, Expression *lemma);

/**
 * Apply the eapply tactic to a goal using a lemma.
 * Time Complexity:
 *
 * @param goal
 * @param lemma
 * @return
 */
TacticResult *engine_tactic_eapply(Expression *goal, Expression *lemma);

/**
 * Apply the exact tactic to solve a goal with a proof term.
 * Time Complexity:
 *
 * @param goal
 * @param proof_term
 * @return
 */
TacticResult *engine_tactic_exact(Expression *goal, Expression *proof_term);

/**
 * Apply the assumption tactic to solve a goal from the context.
 * Time Complexity:
 *
 * @param goal
 * @return
 */
TacticResult *engine_tactic_assumption(Expression *goal);

/**
 * Apply the reflexivity tactic to solve an equality goal.
 * Time Complexity:
 *
 * @param goal
 * @return
 */
TacticResult *engine_tactic_reflexivity(Expression *goal);

/**
 * Apply the split tactic to split a goal into subgoals.
 * Time Complexity:
 *
 * @param goal
 * @return
 */
TacticResult *engine_tactic_split(Expression *goal);

/**
 * Apply the left tactic to choose the left branch of a goal.
 * Time Complexity:
 *
 * @param goal
 * @return
 */
TacticResult *engine_tactic_left(Expression *goal);

/**
 * Apply the right tactic to choose the right branch of a goal.
 * Time Complexity:
 *
 * @param goal
 * @return
 */
TacticResult *engine_tactic_right(Expression *goal);

/**
 * Apply the exists tactic to provide a witness for a goal.
 * Time Complexity:
 *
 * @param goal
 * @param witness
 * @return
 */
TacticResult *engine_tactic_exists(Expression *goal, Expression *witness);

/**
 * Apply the cbv tactic to normalize a goal.
 * Time Complexity:
 *
 * @param goal
 * @param rules
 * @param rule_count
 * @return
 */
TacticResult *engine_tactic_cbv(Expression *goal, char **rules, int rule_count);

/**
 * Apply the rewrite tactic to rewrite a goal using a lemma.
 * Time Complexity:
 *
 * @param goal
 * @param lemma
 * @return
 */
TacticResult *engine_tactic_rewrite(Expression *goal, Expression *lemma);

/**
 * Apply the erewrite tactic to rewrite a goal using a lemma with evar instantiation.
 * Time Complexity:
 *
 * @param goal
 * @param lemma
 * @return
 */
TacticResult *engine_tactic_erewrite(Expression *goal, Expression *lemma);

/**
 * Check whether a tactic result indicates success.
 * Time Complexity:
 *
 * @param result
 * @return
 */
bool engine_tactic_result_success(TacticResult *result);

/**
 * Get the error string from a tactic result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
char *engine_tactic_result_error(TacticResult *result);

/**
 * Get the goals list from a tactic result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
void *engine_tactic_result_goals(TacticResult *result);

/**
 * Free a tactic result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
void engine_tactic_result_free(TacticResult *result);

TacticResult *engine_tactic_result_new(bool success, void *new_goals, char *error_message);
TacticResult *engine_tactic_result_value(TacticValue *value);
TacticValue *engine_tactic_result_get_value(TacticResult *result);
void *engine_tactic_result_take_goals(TacticResult *result);
TacticValue *engine_tactic_result_take_value(TacticResult *result);
void engine_tactic_result_set_goals(TacticResult *result, void *goals);
void engine_tactic_result_set_value(TacticResult *result, TacticValue *value);

TacticValue *engine_tactic_value_expr(Expression *expr);
TacticValue *engine_tactic_value_pair(TacticValue *fst, TacticValue *snd);
TacticValue *engine_tactic_value_ast(struct AST *ast);
TacticValue *engine_tactic_value_dup(TacticValue *value);
void engine_tactic_value_free(TacticValue *value);
EngineTacticValueKind engine_tactic_value_kind(TacticValue *value);
Expression *engine_tactic_value_as_expr(TacticValue *value);
struct AST *engine_tactic_value_as_ast(TacticValue *value);
TacticValue *engine_tactic_value_pair_fst(TacticValue *value);
TacticValue *engine_tactic_value_pair_snd(TacticValue *value);

/* ============================================================================
 * Rewriting Operations
 * ============================================================================ */

/**
 * Rewrite an expression using a lemma under a given context.
 * Time Complexity:
 *
 * @param expr
 * @param lemma
 * @param context
 * @return
 */
RewriteResult *engine_rewrite(Expression *expr, Expression *lemma, Context *context);

/**
 * Rewrite an expression using a lemma with evar instantiation under a given context.
 * Time Complexity:
 *
 * @param expr
 * @param lemma
 * @param context
 * @return
 */
RewriteResult *engine_erewrite(Expression *expr, Expression *lemma, Context *context);

/**
 * Get the original expression from a rewrite result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
Expression *engine_rewrite_result_original(RewriteResult *result);

/**
 * Get the rewritten expression from a rewrite result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
Expression *engine_rewrite_result_rewritten(RewriteResult *result);

/**
 * Get the goals list from a rewrite result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
void *engine_rewrite_result_goals(RewriteResult *result);

/**
 * Free a rewrite result.
 * Time Complexity:
 *
 * @param result
 * @return
 */
void engine_rewrite_result_free(RewriteResult *result);

#endif  // ENGINE_API_H
