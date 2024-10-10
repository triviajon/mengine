#ifndef LOGIC_H
#define LOGIC_H

#include "kernel/doubly_linked_list.h"
#include "kernel/expression.h"

// Given an expression e, builds the new expression of type (eq e e) using the
// eq_refl constructor of the inductive type "eq"
Expression *build_eq_refl(Expression *e);

// Given expressions f_equality (representing equality between some f and f')
// and x_equality (representing equality between some x and x'), returns an
// expression which says that the application (f' x') is equal to (f x)
Expression *build_app_cong(Expression *f_equality, Expression *x_equality);

// Given expressions x_y_equality (representing equality between some x and y)
// and y_z_equality (representing equality between some y and z), returns an
// expression representing the equality between x and z, i.e., (eq x z).
Expression *build_eq_trans(Expression *x_y_equality, Expression *y_z_equality);

#endif  // LOGIC_H
