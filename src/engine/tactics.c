#include "tactics.h"

void intros(ProofState *proof_state) {
  if (num_holes(proof_state->holes) != 1) {
    // If num_holes is 0, obviously we shouldn't do anything.
    // How do I handle calling intros on a proof_state with more than one
    // hole?
    return;
  }

  Expression *goal_hole = get_hole_at(proof_state->holes, 0);
  // Expression *new_hole = init_hole_expression("Goal",
  // TODO
}

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
    // We should not be responsible for freeing the initial expression
    // free_expression(proof->expr);
    free_expression(proof->rewritten_expr);
    free_expression(proof->equality_proof);
    free(proof);
  }
}

bool nothing_rewritten(RewriteProof *rewrite_proof) {
  return rewrite_proof->expr == rewrite_proof->rewritten_expr;
}

RewriteProof *rewrite_head(Expression *expr, Theorem *theorem) {
  // A simple rewrite-head function would match its input against the left-hand
  // side of a quantified equality theorem, and return (an instantiated version
  // of) the right-hand side and (an instantiated version of) the theorem in
  // case of success.

  if (theorem->theorem->type != FORALL_EXPRESSION) {
    // Nothing to instantiate, but we can probably deal with this too.
    return init_rewrite_proof(expr, expr, build_eq_refl(expr));
  }

  Context *theorem_statement_ctx = theorem->theorem->value.forall.context;
  Expression *theorem_statement_body = theorem->theorem->value.forall.body;

  // Our body should be of the form eq lhs rhs, we want to match expr
  // against exprA to figure out how to instantiate the theorem
  Expression *lhs = eq_lhs(theorem_statement_body);
  // Unification algorithm?
  Context *variable_instantiation = unify(lhs, expr);
  
  // How should I instantiate the theorem now that I know what variables should be where?

  return NULL;
}

RewriteProof *rewrite_app(Expression *expr, Theorem *theorem) {
  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = rewrite(func, theorem);
  RewriteProof *rw_arg_proof = rewrite(arg, theorem);

  Expression *mid, *fx_mid;

  if (nothing_rewritten(rw_func_proof) || nothing_rewritten(rw_arg_proof)) {
    mid = expr;
    fx_mid = init_rewrite_proof(expr, expr, build_eq_refl(expr));
  } else {
    mid = init_app_expression(expr->value.app.context,
                              rw_func_proof->rewritten_expr,
                              rw_arg_proof->rewritten_expr);
    fx_mid = build_app_cong(rw_func_proof->equality_proof,
                            rw_arg_proof->equality_proof);
  }

  RewriteProof *rewritten_mid = rewrite_head(mid, theorem);
  if (nothing_rewritten(rewritten_mid)) {
    free_rewrite_proof(rewritten_mid);
    return init_rewrite_proof(expr, mid, fx_mid);
  } else {
    return init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(fx_mid, rewritten_mid->equality_proof));
  }
}

RewriteProof *rewrite(Expression *expr, Theorem *theorem) {
  switch (expr->type) {
    case (APP_EXPRESSION):
      return rewrite_app(expr, theorem);
  }
}