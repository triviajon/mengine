#include "src/engine/tactics.h"

#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/rewrite_internal.h"
#include "src/engine/tactic_api.h"
#include "src/engine/unify.h"
#include "src/kernel/expression.h"
#include "src/kernel/normalize.h"
#include "src/kernel/subst.h"
#include "src/runtime/core.h"

// Helper that performs a single intro step, returning the new goal on success
// or NULL on failure.
static Expression *intro_step(Expression *goal, char *name, char **error_out) {
    if (goal->tag != HOLE_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a hole";
        }
        return NULL;
    }

    Expression *goal_ty = get_expression_type(goal);
    if (goal_ty->tag != FORALL_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a forall expression";
        }
        return NULL;
    }

    Expression *x = get_forall_bound_variable(goal_ty);
    Expression *A = get_expression_type(x);
    Expression *B = get_forall_body(goal_ty);

    Expression *x_prime = init_var_expression_wc(name, A, get_expression_context(goal));
    Expression *B_prime = new_subst(x_prime, B, x, x_prime);
    Context *new_context = x_prime;
    Expression *new_goal = init_hole_expression("Goal", B_prime, new_context);

    Expression *proof_of_original = init_lambda_expression_wc(x_prime, new_goal);
    if (can_fill(goal, proof_of_original)) {
        fill_hole(goal, proof_of_original);
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
    if (goal->tag != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL, "Could not unify lemma with goal");
    }

    // For apply, we do not allow the unification result to have any remaining
    // open holes.
    if (dll_len(unif_result->new_goals) != 0) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Apply tactic does not allow remaining open holes");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!can_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Cannot fill goal with lemma instantiation");
    }

    fill_hole(goal, lemma_inst);

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    free_unification_result(unif_result);
    return result;
}

TacticResult *eapply_tactic(Expression *goal, Expression *lemma) {
    if (goal->tag != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL, "Could not unify lemma with goal");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!can_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL, "Cannot fill goal with lemma instantiation");
    }

    fill_hole(goal, lemma_inst);

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    free(unif_result);

    return result;
}

TacticResult *assumption_tactic(Expression *goal) {
    if (goal->tag != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    Context *ctx = get_expression_context(goal);

    // Iterate through the context to find a variable whose type matches the
    // goal
    while (ctx && !context_is_empty(ctx)) {
        Expression *var = ctx;
        if (can_fill(goal, var)) {
            fill_hole(goal, var);
            return init_tactic_result(true, dll_create(), NULL);
        }

        ctx = get_expression_context(var);
    }

    return init_tactic_result(false, NULL, "No assumption matches the goal");
}

TacticResult *exact_tactic(Expression *goal, Expression *proof_term) {
    if (goal->tag != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    if (!can_fill(goal, proof_term)) {
        return init_tactic_result(false, NULL, "Cannot fill goal with proof term");
    }

    fill_hole(goal, proof_term);
    return init_tactic_result(true, dll_create(), NULL);
}

TacticResult *rewrite_tactic(Expression *goal, Expression *lemma) {
    // Assume return_type has form R lhs rhs, so that R is the relation and the
    // type of lhs/rhs are what the relation are over.
    Expression *return_type = get_expression_type(goal);
    Context *operating_ctx = get_expression_context(goal);

    Expression *func1 = get_app_func(return_type);
    if (!func1) {
        return init_tactic_result(false, NULL, "Goal type is not an application");
    }
    Expression *relation_left_hand = get_app_arg(func1);
    Expression *relation_right_hand = get_app_arg(return_type);
    if (!relation_left_hand || !relation_right_hand) {
        return init_tactic_result(false, NULL, "Goal type malformed");
    }
    Expression *func2 = get_app_func(func1);
    if (!func2) {
        return init_tactic_result(false, NULL, "Goal type is not a binary relation");
    }
    Expression *relation = func2;
    Expression *relation_over = get_expression_type(relation_right_hand);

    // Require that Equivalence proof applies to relation
    // TODO: Since we are hardcoding rewriting only for Leibniz Equality....
    if (get_app_func(relation) != eq) {
        return init_tactic_result(false, NULL,
                                  "Currently only rewriting for Leibniz Equality is supported");
    }

    // Once that's confirmed, we can begin attempting to rewrite. Start with the
    // lhs and try to apply the lemma.
    RewriteResult *rwr = rewrite(relation_left_hand, lemma, operating_ctx);
    if (!rwr) {
        return init_tactic_result(false, NULL, "Rewriting failed");
    }

    // rwr gives us eq A lhs lhs' with proof pf. We now need to build the proof
    // of eq relation_over lhs rhs. We'll just use eq_trans : (forall (A: Type),
    // (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x)
    // y)), (forall (_: (((eq A) y) z)), (((eq A) x) z)))))))
    Expression *new_goal_type = init_app_expression_wc(
        init_app_expression_wc(init_app_expression_wc(eq, relation_over, operating_ctx),
                               rwr->rewritten, operating_ctx),
        relation_right_hand, operating_ctx);
    Expression *new_goal = init_hole_expression("Goal", new_goal_type, operating_ctx);
    Expression *proof_of_goal = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(eq_trans, relation_over, operating_ctx),
                        rwr->original, operating_ctx),
                    rwr->rewritten, operating_ctx),
                relation_right_hand, operating_ctx),
            rwr->original_to_rewritten_proof, operating_ctx),
        new_goal, operating_ctx);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    new_goals = dll_merge(new_goals, rwr->new_goals);

    if (!can_fill(goal, proof_of_goal)) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Failed to fill the goal after rewriting");
    }

    fill_hole(goal, proof_of_goal);

    TacticResult *tac_result = init_tactic_result(true, new_goals, NULL);
    free_rewrite_result(rwr);
    return tac_result;
}

TacticResult *erewrite_tactic(Expression *goal, Expression *lemma) {
    // Assume return_type has form R lhs rhs, so that R is the relation and the
    // type of lhs/rhs are what the relation are over.
    Expression *return_type = get_expression_type(goal);
    Context *operating_ctx = get_expression_context(goal);

    Expression *func1 = get_app_func(return_type);
    if (!func1) {
        return init_tactic_result(false, NULL, "Goal type is not an application");
    }
    Expression *relation_left_hand = get_app_arg(func1);
    Expression *relation_right_hand = get_app_arg(return_type);
    if (!relation_left_hand || !relation_right_hand) {
        return init_tactic_result(false, NULL, "Goal type malformed");
    }
    Expression *func2 = get_app_func(func1);
    if (!func2) {
        return init_tactic_result(false, NULL, "Goal type is not a binary relation");
    }
    Expression *relation = func2;
    Expression *relation_over = get_expression_type(relation_right_hand);

    // Require that Equivalence proof applies to relation
    // TODO: Since we are hardcoding rewriting only for Leibniz Equality....
    if (get_app_func(relation) != eq) {
        return init_tactic_result(false, NULL,
                                  "Currently only rewriting for Leibniz Equality is supported");
    }

    // Once that's confirmed, we can begin attempting to rewrite. Start with the
    // lhs and try to apply the lemma.
    RewriteResult *rwr = erewrite(relation_left_hand, lemma, operating_ctx);
    if (!rwr) {
        return init_tactic_result(false, NULL, "Rewriting failed");
    }

    // rwr gives us eq A lhs lhs' with proof pf. We now need to build the proof
    // of eq relation_over lhs rhs. We'll just use eq_trans : (forall (A: Type),
    // (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x)
    // y)), (forall (_: (((eq A) y) z)), (((eq A) x) z)))))))
    Expression *new_goal_type = init_app_expression_wc(
        init_app_expression_wc(init_app_expression_wc(eq, relation_over, operating_ctx),
                               rwr->rewritten, operating_ctx),
        relation_right_hand, operating_ctx);
    Expression *new_goal = init_hole_expression("Goal", new_goal_type, operating_ctx);
    Expression *proof_of_goal = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(eq_trans, relation_over, operating_ctx),
                        rwr->original, operating_ctx),
                    rwr->rewritten, operating_ctx),
                relation_right_hand, operating_ctx),
            rwr->original_to_rewritten_proof, operating_ctx),
        new_goal, operating_ctx);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    new_goals = dll_merge(new_goals, rwr->new_goals);

    if (!can_fill(goal, proof_of_goal)) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL, "Failed to fill the goal after rewriting");
    }

    fill_hole(goal, proof_of_goal);

    TacticResult *tac_result = init_tactic_result(true, new_goals, NULL);
    free_rewrite_result(rwr);
    return tac_result;
}

TacticResult *cbv_tactic(Expression *goal, char **rules, int rule_count) {
    ReductionFlags flags = 0;
    if (!rules || rule_count == 0) {
        flags = REDUCE_ALL;
    } else {
        for (int i = 0; i < rule_count; i++) {
            if (strcmp(rules[i], "beta")) {
                flags |= REDUCE_BETA;
            } else if (strcmp(rules[i], "delta")) {
                flags |= REDUCE_DELTA;
            } else if (strcmp(rules[i], "iota")) {
                flags |= REDUCE_IOTA;
            } else if (strcmp(rules[i], "fix")) {
                flags |= REDUCE_FIX;
            } else {
                // unknown rule for now
                return init_tactic_result(false, NULL, "Unknown converison rule in cbv.");
            }
        }
    }

    char *old_goal_name = get_hole_name(goal);
    Expression *old_goal_ty = get_expression_type(goal);
    Context *ctx = get_expression_context(goal);

    Expression *new_goal_ty = normalize_cbv(old_goal_ty, flags);
    Expression *new_goal = init_hole_expression(old_goal_name, new_goal_ty, ctx);

    if (!can_fill(goal, new_goal)) {
        return init_tactic_result(false, NULL, "Failed to fill the goal conversion");
    }

    fill_hole(goal, new_goal);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    return init_tactic_result(true, new_goals, NULL);
}