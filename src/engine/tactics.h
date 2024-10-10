#ifndef TACTICS_H
#define TACTICS_H

#include "logic.h"
#include "proof_state.h"
#include "theorem.h"
#include "unify.h"

void intros(ProofState *proof_state);

typedef struct {
  Expression *expr;
  Expression *rewritten_expr;
  Expression *equality_proof;
} RewriteProof;

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof);
void free_rewrite_proof(RewriteProof *proof);

#endif  // TACTICS_H
