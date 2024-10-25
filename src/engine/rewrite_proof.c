#include "rewrite_proof.h"

RewriteProof *init_rewrite_proof(Expression *expr, Expression *rewritten_expr,
                                 Expression *equality_proof) {
  RewriteProof *proof = malloc(sizeof(RewriteProof));
  proof->expr = expr;
  proof->rewritten_expr = rewritten_expr;
  proof->equality_proof = equality_proof;
  return proof;
}

void free_rewrite_proof(RewriteProof *proof) {
  if (proof) {
    // We should not be responsible for freeing the initial expression. 
    // free_expression(proof->expr);
    // free_expression(proof->rewritten_expr);
    // free_expression(proof->equality_proof);
    // free(proof);
  }
}
