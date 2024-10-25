#ifndef REWRITE_PROOF_H
#define REWRITE_PROOF_H

#include "expression.h"

typedef struct {
  Expression *expr;
  Expression *rewritten_expr;
  Expression *equality_proof;
} RewriteProof;

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof);
void free_rewrite_proof(RewriteProof *proof);

#endif  // REWRITE_PROOF_H
