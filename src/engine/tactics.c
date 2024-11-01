#include "tactics.h"

void intros(ProofState *proof_state) {
  // if (num_holes(proof_state->holes) != 1) {
  //   // If num_holes is 0, obviously we shouldn't do anything.
  //   // How do I handle calling intros on a proof_state with more than one
  //   // hole?
  //   return;
  // }

  // Expression *goal_hole = get_hole_at(proof_state->holes, 0);
  // // Expression *new_hole = init_hole_expression("Goal",
  // // TODO
}

bool nothing_rewritten(RewriteProof *rewrite_proof) {
  return rewrite_proof == NULL ||
         rewrite_proof->expr == rewrite_proof->rewritten_expr;
}

bool rewrite_failed(RewriteProof *rewrite_proof) {
  return rewrite_proof == NULL;
}

// RewriteProof *rewrite_head(Expression *expr, Theorem *theorem) {
//   // A simple rewrite-head function would match its input against the
//   left-hand
//   // side of a quantified equality theorem, and return (an instantiated
//   version
//   // of) the right-hand side and (an instantiated version of) the theorem in
//   // case of success.

//   if (theorem->theorem->type != FORALL_EXPRESSION) {
//     // Nothing to instantiate, but we can probably deal with this too.
//     return init_rewrite_proof(expr, expr, build_eq_refl(expr));
//   }

//   Context *theorem_statement_ctx = theorem->theorem->value.forall.context;
//   Expression *theorem_statement_body = theorem->theorem->value.forall.body;

//   // Our body should be of the form eq lhs rhs, we want to match expr
//   // against lhs to figure out how to instantiate the theorem
//   Expression *lhs = eq_lhs(theorem_statement_body);
//   // Unification algorithm?
//   Context *variable_instantiation = unify(lhs, expr);

//   // How should I instantiate the theorem now that I know what variables
//   should
//   // be where?
//   // TODO
//   return NULL;
// }

bool expr_match(Expression *expr1, Expression *expr2) {
  switch (expr1->type) {
    case VAR_EXPRESSION:
      if (expr2->type == VAR_EXPRESSION) {
        return expr1 == expr2;
      }
      break;
    case LAMBDA_EXPRESSION:
      if (expr2->type == LAMBDA_EXPRESSION) {
        return expr_match(expr1->value.lambda.body, expr2->value.lambda.body);
      }
      break;
    case APP_EXPRESSION:
      if (expr2->type == APP_EXPRESSION) {
        return expr_match(expr1->value.app.func, expr2->value.app.func) &&
               expr_match(expr1->value.app.arg, expr2->value.app.arg);
      }
      break;
    default:
      break;
  }

  return false;
}

// RewriteProof *rewrite_app(Expression *expr, Theorem *theorem) {
//   Expression *func = expr->value.app.func;
//   Expression *arg = expr->value.app.arg;
//   RewriteProof *rw_func_proof = rewrite(func, theorem);
//   RewriteProof *rw_arg_proof = rewrite(arg, theorem);

//   Expression *mid, *fx_mid;

//   if (nothing_rewritten(rw_func_proof) || nothing_rewritten(rw_arg_proof)) {
//     mid = expr;
//     fx_mid = init_rewrite_proof(expr, expr, build_eq_refl(expr));
//   } else {
//     mid = init_app_expression(expr->value.app.context,
//                               rw_func_proof->rewritten_expr,
//                               rw_arg_proof->rewritten_expr);
//     fx_mid = build_app_cong(rw_func_proof->equality_proof,
//                             rw_arg_proof->equality_proof);
//   }

//   RewriteProof *rewritten_mid = rewrite_head(mid, theorem);
//   if (nothing_rewritten(rewritten_mid)) {
//     free_rewrite_proof(rewritten_mid);
//     return init_rewrite_proof(expr, mid, fx_mid);
//   } else {
//     return init_rewrite_proof(
//         expr, rewritten_mid->rewritten_expr,
//         build_eq_trans(fx_mid, rewritten_mid->equality_proof));
//   }
// }

// RewriteProof *rewrite(Expression *expr, Theorem *theorem) {
//   switch (expr->type) {
//     case (APP_EXPRESSION):
//       return rewrite_app(expr, theorem);
//   }
// }

// Example:

RewriteProof *rewrite_head_example(Expression *expr) {
  // Has built into it the lemma (f a = a).
  // We want to match expr against (f a)
  Expression *lhs = build_fa();
  Expression *rhs = a;
  if (expr_match(lhs, expr)) {
    // TODO: Review
    RewriteProof *equality = init_rewrite_proof(expr, rhs, eq_fa_a);
    return equality;
  } else {
    return init_rewrite_proof(expr, expr, build_eq_refl(expr));
  }
}

RewriteProof *rewrite_app_example(Expression *expr) {
  Context *app_context = get_expression_context(expr);

  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = rewrite_example(func);
  RewriteProof *rw_arg_proof = rewrite_example(arg);

  // Expression *mid;
  // RewriteProof *fx_mid;
  RewriteProof *mid_rewrite_proof;

  if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
    mid_rewrite_proof = init_rewrite_proof(expr, expr, build_eq_refl(expr));
  } else {
    mid_rewrite_proof = init_rewrite_proof(
        expr,
        init_app_expression(expr->value.app.context,
                            rw_func_proof->rewritten_expr,
                            rw_arg_proof->rewritten_expr),
        build_f_equal(app_context, rw_func_proof->expr, rw_arg_proof));
  }

  Expression *mid = mid_rewrite_proof->rewritten_expr;
  Expression *fx_mid = mid_rewrite_proof->equality_proof;

  RewriteProof *rewritten_mid = rewrite_head_example(mid);
  if (nothing_rewritten(rewritten_mid)) {
    free_rewrite_proof(rewritten_mid);
    return init_rewrite_proof(expr, mid, fx_mid);
  } else {
    return init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(app_context, mid_rewrite_proof, rewritten_mid));
  }
}

RewriteProof *rewrite_example(Expression *expr) {
  switch (expr->type) {
    case (APP_EXPRESSION):
      return rewrite_app_example(expr);
    case (VAR_EXPRESSION):
      return init_rewrite_proof(expr, expr, build_eq_refl(expr));
    default:
      return NULL; // TODO: Unsupported.
  }
}