#ifndef TACTICS_H
#define TACTICS_H

#include "axiom.h"
#include "beta_reduction.h"
#include "logic.h"
#include "proof_state.h"
#include "rewrite_proof.h"
#include "unify.h"

RewriteProof *rewrite_head(Expression *expr, Expression *lemma);
RewriteProof *rewrite(Context *ctx, Expression *expr, Expression *lemma);

#endif  // TACTICS_H
