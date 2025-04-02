#ifndef SUBST_H
#define SUBST_H

#include "beta_reduction.h"
#include "doubly_linked_list.h"
#include "expression.h"
#include "context.h"

// Kicks off a substitution in expression. Replaces all instances of old_e with
// new_e.
Expression *subst(Expression *expression, Expression *old_e, Expression *new_e);

// Kicks of multiple substitutions to be performed at once in expression. If
// old_exprs is a list of variables to be replaced, the new_exprs is a list of
// the same length with the replacements. I.e., old_exprs[i] will be replaced
// with new_exprs[i].
//
// Parallel substitution arises naturally when considering contexts. For
// example, if the user kicks off a substitution of [A -> B] in "forall x: A, eq
// A x x", then we must also simulatenously replace x with a fresh variable "x':
// B" in order for the expression to always remain type-checkable.
Expression *p_subst(Expression *expression, DoublyLinkedList *old_exprs,
                    DoublyLinkedList *new_exprs);

#endif // SUBST_H