#include "tactics.h"

DoublyLinkedList *apply(Expression *goal, Expression *lemma) {
    UnificationResult *unification_result = eunify2(lemma, goal);
    Expression *instantiated_lemma = unification_result->lemma_instantiation;
    DoublyLinkedList *new_goals = unification_result->new_goals;
    if (can_fill(goal, instantiated_lemma)) {
        fill_hole(goal, instantiated_lemma);
        return new_goals;
    }
    return NULL;
}

DoublyLinkedList *eapply(Expression *goal, Expression *lemma) {
    UnificationResult *unification_result = eunify2(lemma, goal);
    Expression *instantiated_lemma = unification_result->lemma_instantiation;
    DoublyLinkedList *new_goals = unification_result->new_goals;
    if (can_fill(goal, instantiated_lemma)) {
        fill_hole(goal, instantiated_lemma);
        return new_goals;
    }
    return NULL;
}

Expression *eexists(Expression *goal) {
    if (goal->type != HOLE_EXPRESSION) return NULL;

    Expression *goal_ty = get_expression_type(goal);
    if (get_innermost_func(goal_ty) != ex) return NULL;
    DoublyLinkedList *remaining_goals = eapply(goal, ex_intro);

    if (!remaining_goals || dll_len(remaining_goals) != 2) {
        fprintf(stderr,
                "Error: Expected exactly two goals after applying eexists.\n");
        return NULL;
    }

    Expression *new_goal = dll_at(remaining_goals, 1)->data;
    return new_goal;
}

IntroReturn *intro(Expression *goal) {
    if (goal->type != HOLE_EXPRESSION) return NULL;

    Expression *goal_ty = get_expression_type(goal);
    if (goal_ty->type != FORALL_EXPRESSION) return NULL;

    Expression *goal_ty_bv = goal_ty->value.forall.bound_variable;
    Expression *goal_ty_body = goal_ty->value.forall.body;

    Context *new_context =
        context_insert(get_expression_context(goal), goal_ty_bv);

    Expression *new_goal =
        init_hole_expression("Goal", goal_ty_body, new_context);
    Expression *proof_of_original =
        init_lambda_expression(goal_ty_bv, new_goal);

    if (can_fill(goal, proof_of_original)) {
        fill_hole(goal, proof_of_original);
        return init_intro_return(goal, new_goal, proof_of_original);
    }
    return NULL;
}