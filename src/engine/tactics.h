#ifndef TACTICS_H
#define TACTICS_H

#include "src/engine/axiom.h"
#include "src/engine/logic.h"
#include "src/engine/rewrite_proof.h"
#include "src/engine/rewrites.h"
#include "src/engine/unify.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"

DoublyLinkedList *apply(Expression *goal, Expression *lemma);
DoublyLinkedList *eapply(Expression *goal, Expression *lemma);
Expression *eexists(Expression *goal);

IntroReturn *intro(Expression *old_proof);

#endif  // TACTICS_H
