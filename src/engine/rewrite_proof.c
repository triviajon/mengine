#include "rewrite_proof.h"

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof) {
  RewriteProof *proof = malloc(sizeof(RewriteProof));
  proof->expr = expr;
  proof->rewritten_expr = rewritten_expr;
  proof->equality_proof = equality_proof;
  return proof;
}

void free_rewrite_proof(RewriteProof *proof) {
  if (proof) {
    free(proof);
  }
}

RewrittenGoal *init_rewritten_goal(Expression *new_goal, Expression *proof) {
  RewrittenGoal *rewritten_goal = malloc(sizeof(RewrittenGoal));
  rewritten_goal->new_goal = new_goal;
  rewritten_goal->proof = proof;
  return rewritten_goal;
}

void free_rewritten_goal(RewrittenGoal *rewritten_goal) {
  if (rewritten_goal) {
    free(rewritten_goal);
  }
}


IntroReturn *init_intro_return(Expression *old_proof, Expression *new_proof, Expression *unsolved_goal) {
  IntroReturn *intro_return = malloc(sizeof(IntroReturn));
  intro_return->old_proof = old_proof;
  intro_return->new_proof = new_proof;
  intro_return->unsolved_goal =unsolved_goal;
  return intro_return;
}

void free_intro_return(IntroReturn *intro_return) {
  if (intro_return) {
    free(intro_return);
  }
}


IntrosReturn *init_intros_return(DoublyLinkedList *hypotheses, Expression *old_proof, Expression *new_proof, Expression *unsolved_goal) {
  IntrosReturn *intros_return = malloc(sizeof(IntrosReturn));
  intros_return->hypotheses = hypotheses;
  intros_return->old_proof = old_proof;
  intros_return->new_proof = new_proof;
  intros_return->unsolved_goal =unsolved_goal;
  return intros_return;
}

void free_intros_return(IntrosReturn *intros_return) {
  if (intros_return) {
    free(intros_return);
  }
}