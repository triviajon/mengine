#ifndef TACTICS_H
#define TACTICS_H

#include "axiom.h"
#include "beta_reduction.h"
#include "context.h"
#include "logic.h"
#include "proof_state.h"
#include "rewrite_proof.h"
#include "unify.h"

int get_rewrite_cache_hits();
int get_rewrite_locations();

RewriteProof *_rewrite(Expression *expr, Expression *lemma);
void clear_rewrite_proofs(Expression *expr);

RewriteProof *rewrite_head(Expression *expr, Expression *lemma);
RewriteProof *rewrite(Expression *expr, Expression *lemma);

#endif  // TACTICS_H
