#include "src/engine/tactics.h"

#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/rewrite_internal.h"
#include "src/engine/tactic_api.h"
#include "src/engine/unify.h"
#include "src/kernel/kernel_api.h"
#include "src/runtime/core.h"

// Helper that performs a single intro step, returning the new goal on success
// or NULL on failure.
static Expression *intro_step(Expression *goal, char *name, char **error_out) {
    if (!kernel_expr_is_hole(goal)) {
        if (error_out) {
            *error_out = "Goal is not a hole";
        }
        return NULL;
    }

    Expression *goal_ty = kernel_expr_type(goal);
    if (kernel_forall_var(goal_ty) == NULL) {
        if (error_out) {
            *error_out = "Goal is not a forall expression";
        }
        return NULL;
    }

    Expression *x = kernel_forall_var(goal_ty);
    Expression *A = kernel_expr_type(x);
    Expression *B = kernel_forall_body(goal_ty);

    // If no name provided, use the bound variable name from the forall type
    char *intro_name = name ? name : kernel_var_name(x);
    Expression *x_prime = kernel_var_create(intro_name, A, kernel_expr_context(goal));
    Expression *B_prime = kernel_subst(x_prime, B, x, x_prime);
    Context *new_context = x_prime;
    Expression *new_goal = kernel_hole_create((char *)"Goal", B_prime, new_context);

    Expression *proof_of_original = kernel_lambda_create(x_prime, new_goal);
    if (kernel_hole_fill(goal, proof_of_original)) {
        return new_goal;
    }

    if (error_out) {
        *error_out = "Failed to fill the hole";
    }
    return NULL;
}

TacticResult *intro_tactic(Expression *goal, char *name) {
    char *error = NULL;
    Expression *new_goal = intro_step(goal, name, &error);

    if (!new_goal) {
        return init_tactic_result(false, NULL, error);
    }

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    return init_tactic_result(true, new_goals, NULL);
}

TacticResult *intros_tactic(Expression *goal, char **names, size_t name_count) {
    if (name_count == 0) {
        return init_tactic_result(false, NULL, "intros requires at least one name");
    }

    Expression *current_goal = goal;
    char *error = NULL;

    for (size_t i = 0; i < name_count; i++) {
        Expression *next_goal = intro_step(current_goal, names[i], &error);
        if (!next_goal) {
            return init_tactic_result(false, NULL, error);
        }
        current_goal = next_goal;
    }

    // Return only the final goal
    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(current_goal));
    return init_tactic_result(true, new_goals, NULL);
}

TacticResult *apply_tactic(Expression *goal, Expression *lemma) {
    if (!kernel_expr_is_hole(goal)) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL, "Could not unify lemma with goal");
    }

    // For apply, we do not allow the unification result to have any remaining
    // open holes.
    if (unif_result->new_goals->head != NULL) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Apply tactic does not allow remaining open holes");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!kernel_hole_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Cannot fill goal with lemma instantiation");
    }

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    unif_result->new_goals = NULL;  // transferred to tactic result
    free_unification_result(unif_result);
    return result;
}

TacticResult *eapply_tactic(Expression *goal, Expression *lemma) {
    if (!kernel_expr_is_hole(goal)) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL, "Could not unify lemma with goal");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!kernel_hole_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Cannot fill goal with lemma instantiation");
    }

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    unif_result->new_goals = NULL;  // transferred to tactic result
    free_unification_result(unif_result);

    return result;
}

TacticResult *assumption_tactic(Expression *goal) {
    if (!kernel_expr_is_hole(goal)) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    Context *ctx = kernel_expr_context(goal);

    // Iterate through the context to find a variable whose type matches the
    // goal
    while (ctx && kernel_context_size(ctx) > 0) {
        Expression *var = ctx;
        if (kernel_hole_fill(goal, var)) {
            return init_tactic_result(true, dll_create(), NULL);
        }

        ctx = kernel_expr_context(var);
    }

    return init_tactic_result(false, NULL, "No assumption matches the goal");
}

TacticResult *exact_tactic(Expression *goal, Expression *proof_term) {
    if (!kernel_expr_is_hole(goal)) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    if (!kernel_hole_fill(goal, proof_term)) {
        return init_tactic_result(false, NULL, "Cannot fill goal with proof term");
    }

    return init_tactic_result(true, dll_create(), NULL);
}

TacticResult *rewrite_tactic(Expression *goal, Expression *lemma) {
    // Assume return_type has form R lhs rhs, so that R is the relation and the
    // type of lhs/rhs are what the relation are over.
    Expression *return_type = kernel_expr_type(goal);
    Context *operating_ctx = kernel_expr_context(goal);

    Expression *func1 = kernel_app_func(return_type);
    if (!func1) {
        return init_tactic_result(false, NULL, "Goal type is not an application");
    }
    Expression *relation_left_hand = kernel_app_arg(func1);
    Expression *relation_right_hand = kernel_app_arg(return_type);
    if (!relation_left_hand || !relation_right_hand) {
        return init_tactic_result(false, NULL, "Goal type malformed");
    }
    Expression *func2 = kernel_app_func(func1);
    if (!func2) {
        return init_tactic_result(false, NULL, "Goal type is not a binary relation");
    }
    Expression *relation = func2;
    Expression *relation_over = kernel_expr_type(relation_right_hand);

    // Require that Equivalence proof applies to relation
    // TODO: Since we are hardcoding rewriting only for Leibniz Equality....
    if (kernel_app_func(relation) != eq) {
        return init_tactic_result(false, NULL,
                                  "Currently only rewriting for Leibniz Equality is supported");
    }

    // Once that's confirmed, we can begin attempting to rewrite. Start with the
    // lhs and try to apply the lemma.
    RewriteResult *rwr = rewrite(relation_left_hand, lemma, operating_ctx);
    if (!rwr) {
        return init_tactic_result(false, NULL, "Rewriting failed");
    }

    // Check if the rewrite made no progress (noop)
    if (rwr->original == rwr->rewritten) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Rewriting made no progress");
    }

    // rwr gives us eq A lhs lhs' with proof pf. We now need to build the proof
    // of eq relation_over lhs rhs. We'll just use eq_trans : (forall (A: Type),
    // (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x)
    // y)), (forall (_: (((eq A) y) z)), (((eq A) x) z)))))))
    Expression *new_goal_type =
        kernel_app_create(kernel_app_create(kernel_app_create(eq, relation_over, operating_ctx),
                                            rwr->rewritten, operating_ctx),
                          relation_right_hand, operating_ctx);
    Expression *new_goal = kernel_hole_create((char *)"Goal", new_goal_type, operating_ctx);
    Expression *proof_of_goal = kernel_app_create(
        kernel_app_create(
            kernel_app_create(
                kernel_app_create(
                    kernel_app_create(kernel_app_create(eq_trans, relation_over, operating_ctx),
                                      rwr->original, operating_ctx),
                    rwr->rewritten, operating_ctx),
                relation_right_hand, operating_ctx),
            rwr->original_to_rewritten_proof, operating_ctx),
        new_goal, operating_ctx);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    if (rwr->new_goals) {
        new_goals = dll_merge(new_goals, rwr->new_goals);
        rwr->new_goals = NULL;  // transferred ownership
    }

    if (!kernel_hole_fill(goal, proof_of_goal)) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Failed to fill the goal after rewriting");
    }

    TacticResult *tac_result = init_tactic_result(true, new_goals, NULL);
    free_rewrite_result(rwr);
    return tac_result;
}

TacticResult *erewrite_tactic(Expression *goal, Expression *lemma) {
    // Assume return_type has form R lhs rhs, so that R is the relation and the
    // type of lhs/rhs are what the relation are over.
    Expression *return_type = kernel_expr_type(goal);
    Context *operating_ctx = kernel_expr_context(goal);

    Expression *func1 = kernel_app_func(return_type);
    if (!func1) {
        return init_tactic_result(false, NULL, "Goal type is not an application");
    }
    Expression *relation_left_hand = kernel_app_arg(func1);
    Expression *relation_right_hand = kernel_app_arg(return_type);
    if (!relation_left_hand || !relation_right_hand) {
        return init_tactic_result(false, NULL, "Goal type malformed");
    }
    Expression *func2 = kernel_app_func(func1);
    if (!func2) {
        return init_tactic_result(false, NULL, "Goal type is not a binary relation");
    }
    Expression *relation = func2;
    Expression *relation_over = kernel_expr_type(relation_right_hand);

    // Require that Equivalence proof applies to relation
    // TODO: Since we are hardcoding rewriting only for Leibniz Equality....
    if (kernel_app_func(relation) != eq) {
        return init_tactic_result(false, NULL,
                                  "Currently only rewriting for Leibniz Equality is supported");
    }

    // Once that's confirmed, we can begin attempting to rewrite. Start with the
    // lhs and try to apply the lemma.
    RewriteResult *rwr = erewrite(relation_left_hand, lemma, operating_ctx);
    if (!rwr) {
        return init_tactic_result(false, NULL, "Rewriting failed");
    }

    // Check if the rewrite made no progress (noop)
    if (rwr->original == rwr->rewritten) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Rewriting made no progress");
    }

    // rwr gives us eq A lhs lhs' with proof pf. We now need to build the proof
    // of eq relation_over lhs rhs. We'll just use eq_trans : (forall (A: Type),
    // (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x)
    // y)), (forall (_: (((eq A) y) z)), (((eq A) x) z)))))))
    Expression *new_goal_type =
        kernel_app_create(kernel_app_create(kernel_app_create(eq, relation_over, operating_ctx),
                                            rwr->rewritten, operating_ctx),
                          relation_right_hand, operating_ctx);
    Expression *new_goal = kernel_hole_create((char *)"Goal", new_goal_type, operating_ctx);
    Expression *proof_of_goal = kernel_app_create(
        kernel_app_create(
            kernel_app_create(
                kernel_app_create(
                    kernel_app_create(kernel_app_create(eq_trans, relation_over, operating_ctx),
                                      rwr->original, operating_ctx),
                    rwr->rewritten, operating_ctx),
                relation_right_hand, operating_ctx),
            rwr->original_to_rewritten_proof, operating_ctx),
        new_goal, operating_ctx);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    if (rwr->new_goals) {
        new_goals = dll_merge(new_goals, rwr->new_goals);
        rwr->new_goals = NULL;  // transferred ownership
    }

    if (!kernel_hole_fill(goal, proof_of_goal)) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Failed to fill the goal after rewriting");
    }

    TacticResult *tac_result = init_tactic_result(true, new_goals, NULL);
    free_rewrite_result(rwr);
    return tac_result;
}

TacticResult *cbv_tactic(Expression *goal, char **rules, int rule_count) {
    unsigned int flags = 0;
    if (!rules || rule_count == 0) {
        flags = KERNEL_REDUCE_ALL;
    } else {
        for (int i = 0; i < rule_count; i++) {
            if (strcmp(rules[i], "beta") == 0) {
                flags |= KERNEL_REDUCE_BETA;
            } else if (strcmp(rules[i], "delta") == 0) {
                flags |= KERNEL_REDUCE_DELTA;
            } else if (strcmp(rules[i], "iota") == 0) {
                flags |= KERNEL_REDUCE_IOTA;
            } else if (strcmp(rules[i], "fix") == 0) {
                flags |= KERNEL_REDUCE_FIX;
            } else {
                // unknown rule for now
                return init_tactic_result(false, NULL, "Unknown converison rule in cbv.");
            }
        }
    }

    char *old_goal_name = kernel_hole_name(goal);
    Expression *old_goal_ty = kernel_expr_type(goal);
    Context *ctx = kernel_expr_context(goal);

    Expression *new_goal_ty = kernel_normalize_cbv(old_goal_ty, flags);
    Expression *new_goal = kernel_hole_create(old_goal_name, new_goal_ty, ctx);

    if (!kernel_hole_fill(goal, new_goal)) {
        return init_tactic_result(false, NULL, "Failed to fill the goal conversion");
    }

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    return init_tactic_result(true, new_goals, NULL);
}

TacticResult *reflexivity_tactic(Expression *goal) { return apply_tactic(goal, eq_refl); }

TacticResult *split_tactic(Expression *goal) { return eapply_tactic(goal, prop_conj); }

TacticResult *left_tactic(Expression *goal) { return eapply_tactic(goal, or_introl); }

TacticResult *right_tactic(Expression *goal) { return eapply_tactic(goal, or_intror); }

TacticResult *exists_tactic(Expression *goal, Expression *witness) {
    if (!kernel_expr_is_hole(goal)) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    Expression *goal_ty = kernel_expr_type(goal);
    Context *ctx = kernel_expr_context(goal);

    /* goal_ty should be  ex A P  i.e.  (ex A) P */
    Expression *ex_A = kernel_app_func(goal_ty);
    if (!ex_A) {
        return init_tactic_result(false, NULL, "Goal type is not an application");
    }
    Expression *P = kernel_app_arg(goal_ty);
    Expression *A = kernel_app_arg(ex_A);
    if (!A || !P) {
        return init_tactic_result(false, NULL, "Goal type is not a binary application");
    }

    /* Build subgoal: P witness */
    Expression *P_witness = kernel_app_create(P, witness, ctx);
    Expression *subgoal = kernel_hole_create("_", P_witness, ctx);

    /* Build proof term: ex_intro A P witness subgoal */
    Expression *proof = kernel_app_create(
        kernel_app_create(kernel_app_create(kernel_app_create(ex_intro, A, ctx), P, ctx), witness,
                          ctx),
        subgoal, ctx);

    if (!kernel_hole_fill(goal, proof)) {
        return init_tactic_result(false, NULL, "Cannot fill goal with exists proof term");
    }

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(subgoal));
    return init_tactic_result(true, new_goals, NULL);
}