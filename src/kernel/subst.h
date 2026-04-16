#ifndef NEW_SUBST_H
#define NEW_SUBST_H

#include "src/common/doubly_linked_list.h"
#include "src/kernel/expression.h"

// Kicks off a substitution in t, replacing all instances of x with
// a and forming a new expression. t, x, and a must all
// be valid in the provided context.
//
// The context that the final expression is closed under is the context given by
// the following rule:
//  let gamma, a : A, delta := context
//  gamma |- a : A
//  gamma, a : A, delta |- t : T
//  ------------------------------------------------
//  gamma, delta[x -> a] |- t[x -> a] : T[x -> a]
//
// Proof of correctness follows from the "Substitution Lemma" (Theorem 5.11 of
// Brandl, see link: https://hbr.github.io/Lambda-Calculus/cc-tex/cc.pdf)
Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a);

// Internal substitution function that assumes context has already been cut.
// Only use this if you're managing context transformations manually.
Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a);

// Kicks off multiple substitutions to be performed at once in t. If
// old_exprs is a list of variables to be replaced, then new_exprs is a list of
// the same length with the replacements. I.e., old_exprs[i] will be replaced
// with new_exprs[i].
//
// The context parameter should be the result context (analogous to
// gamma+delta[x->a] in the single substitution case).
//
// Parallel substitution arises naturally when considering contexts. For
// example, if the user kicks off a substitution of [A -> B] in "forall x: A, eq
// A x x", then we must also simultaneously replace x with a fresh variable "x':
// B" in order for the expression to always remain type-checkable.
Expression *new_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                        DoublyLinkedList *new_exprs);

// Like new_subst but skips the collect_subtree DFS.  Safe only when the
// substitution target is a lambda bound variable with no accumulated uplinks
// outside the expression being substituted into.  Use in beta_reduce.
Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a);

#endif  // NEW_SUBST_H