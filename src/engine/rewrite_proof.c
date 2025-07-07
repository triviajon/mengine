#include "rewrite_proof.h"

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof,
                                 DoublyLinkedList *remaining_goals) {
    RewriteProof *proof = malloc(sizeof(RewriteProof));
    proof->expr = expr;
    proof->rewritten_expr = rewritten_expr;
    proof->equality_proof = equality_proof;
    proof->remaining_goals = remaining_goals;
    return proof;
}

void free_rewrite_proof(RewriteProof *proof) {
    if (proof) {
        free(proof);
    }
}

RewrittenGoal *init_rewritten_goal(Expression *new_goal,
                                   DoublyLinkedList *remaining_open) {
    RewrittenGoal *rewritten_goal = malloc(sizeof(RewrittenGoal));
    rewritten_goal->new_goal = new_goal;
    rewritten_goal->remaining_open = remaining_open;
    return rewritten_goal;
}

void free_rewritten_goal(RewrittenGoal *rewritten_goal) {
    if (rewritten_goal) {
        free(rewritten_goal);
    }
}

IntroReturn *init_intro_return(Expression *old_goal, Expression *new_goal,
                               Expression *proof_of_old) {
    IntroReturn *intro_return = malloc(sizeof(IntroReturn));
    intro_return->old_goal = old_goal;
    intro_return->new_goal = new_goal;
    intro_return->proof_of_old = proof_of_old;
    return intro_return;
}

void free_intro_return(IntroReturn *intro_return) {
    if (intro_return) {
        free(intro_return);
    }
}