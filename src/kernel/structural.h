#ifndef STRUCTURAL_H
#define STRUCTURAL_H

#include "src/kernel/expression.h"

// This implements the "directly structurally smaller than" relation, and the transitive closure of
// that relation "structurally smaller than".
//
// t is directly structurally smaller than u if:
// - both t and u are of small types and of arity 0;
// - t is of constructor form (c a1 ... an) and u is definitional equal to one aj of arity 0 or one
// instance of one aj of arity > 0
//
// From page 71 of https://wonks.github.io/type-theory-reading-group/papers/proc92.pdf

// Returns true if t is directly structurally smaller than u.
bool term_directly_structurally_smaller_than_arg(Expression *t, Expression *u);

// Returns true if t is structurally smaller than u.
bool term_structurally_smaller_than_arg(Expression *t, Expression *u);

#endif  // STRUCTURAL_H