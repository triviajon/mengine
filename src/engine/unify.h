#ifndef UNIFY_H
#define UNIFY_H

#include "src/kernel/context.h"
#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/dyn_array_map.h"
#include "src/kernel/expression.h"

// Given a lemma of the form "Forall x1: T1, ..., Forall xn: Tn, ..., B",
// returns B with x1...xn in B substitued for hole expressions, and each hole
// expression is defined with the given context
Expression *instantiate_lemma(Context *context, Expression *lemma);

// Returns a DLL of hole expressions in expr
DoublyLinkedList *list_holes(Expression *expr);

typedef struct {
    Expression *lemma_instantiation;
    DoublyLinkedList *new_goals;
} UnificationResult;

UnificationResult *unify_and_instantiate(Context *goal_context, Expression *lemma,
                                         Expression *lemma_ty, Expression *expr);

Expression *instantiate_lemma_with_bindings(Expression *lemma, Expression *lemma_ty, Map *binders);

UnificationResult *init_unification_result(Expression *lemma_instantiation,
                                           DoublyLinkedList *new_goals);
void free_unification_result(UnificationResult *unification_result);

UnificationResult *eunify2(Expression *lemma, Expression *goal);

UnificationResult *bad_unify_for_eq(Context *goal_context, Expression *lemma, Expression *expr);

#endif  // UNIFY_H