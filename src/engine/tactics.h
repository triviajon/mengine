#ifndef TACTICS_H
#define TACTICS_H

#include "axiom.h"
#include "logic.h"
#include "proof_state.h"
#include "rewrite_proof.h"
#include "unify.h"

void intros(ProofState *proof_state);
RewriteProof *rewrite_head_example(Expression *expr);
RewriteProof *rewrite_example(Expression *expr);

#endif  // TACTICS_H
