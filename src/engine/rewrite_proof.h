#ifndef REWRITE_PROOF_H
#define REWRITE_PROOF_H

#include "src/common/doubly_linked_list.h"

typedef struct Expression Expression;

typedef struct RewriteProof {
    Expression *expr;
    Expression *rewritten_expr;
    Expression *equality_proof;
    DoublyLinkedList *remaining_goals;
} RewriteProof;

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof, DoublyLinkedList *remaining_goals);
void free_rewrite_proof(RewriteProof *proof);

typedef struct {
    Expression *new_goal;
    DoublyLinkedList *remaining_open;
} RewrittenGoal;

RewrittenGoal *init_rewritten_goal(Expression *new_goal, DoublyLinkedList *remaining_open);
void free_rewritten_goal(RewrittenGoal *rewritten_goal);

typedef struct {
    Expression *old_goal;
    Expression *new_goal;
    Expression *proof_of_old;
} IntroReturn;

IntroReturn *init_intro_return(Expression *old_goal, Expression *new_goal,
                               Expression *proof_of_old);
void free_intro_return(IntroReturn *intro_return);

#endif  // REWRITE_PROOF_H
