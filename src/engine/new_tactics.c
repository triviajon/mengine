#include "src/engine/new_tactics.h"
#include "src/engine/unify.h"
#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/expression.h"
#include "src/kernel/new_subst.h"

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message) {
    TacticResult *result = malloc(sizeof(TacticResult));
    if (!result) {
        return NULL;
    }
    
    result->success = success;
    result->new_goals = new_goals;
    result->error_message = error_message;
    
    return result;
}


void free_tactic_result(TacticResult *result) {
    if (!result) return;
    free(result);
}

// Helper that performs a single intro step, returning the new goal on success or NULL on failure.
static Expression *intro_step(Expression *goal, char *name, char **error_out) {
    if (goal->type != HOLE_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a hole";
        }
        return NULL;
    }

    Expression *goal_ty = get_expression_type(goal);
    if (goal_ty->type != FORALL_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a forall expression";
        }
        return NULL;
    }

    Expression *x = goal_ty->value.forall.bound_variable;
    Expression *A = get_expression_type(x);
    Expression *B = goal_ty->value.forall.body;

    Expression *x_prime = init_var_expression_wc(name, A, get_expression_context(goal));
    Expression *B_prime = new_subst(B, x, x_prime);
    Context *new_context = context_insert(get_expression_context(goal), x_prime);
    Expression *new_goal = init_hole_expression("Goal", B_prime, new_context);

    Expression *proof_of_original = init_lambda_expression_wc(x_prime, new_goal, new_context);
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
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL, "Could not unify lemma with goal");
    }

    // For apply, we do not allow the unification result to have any remaining open holes.
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
    if (goal->type != HOLE_EXPRESSION) {
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
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    Context *ctx = get_expression_context(goal);

    // Iterate through the context to find a variable whose type matches the goal
    while (ctx && !context_is_empty(ctx)) {
        Expression *var = ctx->var_type;

        if (can_fill(goal, var)) {
            fill_hole(goal, var);
            return init_tactic_result(true, dll_create(), NULL);
        }

        ctx = ctx->parent;
    }

    return init_tactic_result(false, NULL, "No assumption matches the goal");
}

TacticResult *exact_tactic(Expression *goal, Expression *proof_term) {
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    if (!can_fill(goal, proof_term)) {
        return init_tactic_result(false, NULL, "Cannot fill goal with proof term");
    }

    fill_hole(goal, proof_term);
    return init_tactic_result(true, dll_create(), NULL);
}
