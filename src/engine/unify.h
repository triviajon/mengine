#ifndef UNIFY_H
#define UNIFY_H

#include "axiom.h"
#include "context.h"
#include "doubly_linked_list.h"
#include "dyn_array_map.h"
#include "expression.h"
#include "subst.h"

Expression *get_type_eq(Expression *eq_type);

// Returns the lhs of an equality The equality expresison is an opaque
// reference, and exist in the given context. If the context contains
// eq_expression, then its type should be "(eq lhs) rhs", and this function
// returns lhs. If any steps fails, return NULL.
Expression *get_lhs_eq(Expression *eq_type);

// Returns the rhs of an equality The equality expresison is an opaque
// reference, and exist in the given context. If the context contains
// eq_expression, then its type should be "(eq lhs) rhs", and this function
// returns rhs. If any steps fails, return NULL.
Expression *get_rhs_eq(Expression *eq_type);

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

UnificationResult *unify_and_instantiate(Context *goal_context,
                                         Expression *lemma,
                                         Expression *lemma_ty,
                                         Expression *expr);

Expression *instantiate_lemma_with_bindings(Expression *lemma,
                                            Expression *lemma_ty, Map *binders);

UnificationResult *init_unification_result(Expression *lemma_instantiation,
                                           DoublyLinkedList *new_goals);
void free_unification_result(UnificationResult *unification_result);

UnificationResult *eunify(Expression *lemma, Expression *goal);
UnificationResult *eunify2(Expression *lemma, Expression *goal);

#endif  // UNIFY_H