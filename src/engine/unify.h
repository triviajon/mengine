#ifndef UNIFY_H
#define UNIFY_H

#include "axiom.h"

#include "expression.h"
#include "context.h"
#include "dyn_array_map.h"
#include "doubly_linked_list.h"

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

// Returns true iff the expr is or contains an expression with type HOLE_EXPRESSION
bool contains_holes(Expression *expr);

// Returns a DLL of hole expressions in expr
DoublyLinkedList *list_holes(Expression *expr);

// Create a mapping of holes in exprA to terms in exprB
Map *unify(Context *ctx, Expression *exprA, Expression *exprB);


Expression *instantiate_lemma_with_bindings(Context *ctx, Expression *lemma, Expression *lemma_ty, Map *binders);

#endif  // UNIFY_H