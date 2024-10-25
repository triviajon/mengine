#ifndef LOGIC_H
#define LOGIC_H

#include "axiom.h"
#include "doubly_linked_list.h"
#include "expression.h"
#include "rewrite_proof.h"

// Given an expression e, builds the new expression of type (eq e e) using the
// eq_refl constructor of the inductive type "eq"
Expression *build_eq_refl(Expression *e);

// Given expressions f, and x_equality (representing equality between some x and x'), returns an
// expression which says that the application (f x') is equal to (f x)
Expression *build_f_equal(Context *app_context, Expression *f, RewriteProof *x_equality);

// Given expressions x_y_equality (representing equality between some x and y)
// and y_z_equality (representing equality between some y and z), returns an
// expression representing the equality between x and z, i.e., (eq x z).
Expression *build_eq_trans(Context *app_context, RewriteProof *x_y_equality, RewriteProof *y_z_equality);

#endif  // LOGIC_H
