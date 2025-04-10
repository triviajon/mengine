#ifndef TACTICS_H
#define TACTICS_H

#include "axiom.h"
#include "beta_reduction.h"
#include "context.h"
#include "logic.h"
#include "proof_state.h"
#include "rewrite_proof.h"
#include "unify.h"
#include "rewrites.h"

DoublyLinkedList *apply(Expression *goal, Expression *lemma);
DoublyLinkedList *eapply(Expression *goal, Expression *lemma); 

IntroReturn *intro(Expression *old_proof);

#endif  // TACTICS_H
