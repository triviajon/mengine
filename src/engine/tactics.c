#include "tactics.h"

bool nothing_rewritten(RewriteProof *rewrite_proof) {
  return rewrite_proof == NULL ||
         rewrite_proof->expr == rewrite_proof->rewritten_expr;
}

bool rewrite_failed(RewriteProof *rewrite_proof) {
  return rewrite_proof == NULL;
}

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

// Returns the lhs of an equality The equality expresison is an opaque
// reference, and exist in the given context. If the context contains
// eq_expression, then its type should be "(eq lhs) rhs", and this function
// returns lhs. If any steps fails, return NULL.
Expression *get_lhs_eq(Context *context, Expression *eq_expression) {
  Expression *eq_type = context_lookup(context, eq_expression);
  if (eq_type == NULL) {
    return NULL;
  }
  if (eq_type->type != APP_EXPRESSION) {
    return NULL;
  }
  if (eq_type->value.app.func->type != APP_EXPRESSION) {
    return NULL;
  }
  Expression *expected_eq = eq_type->value.app.func->value.app.func->value.app.func;
  if (expected_eq != eq) {
    return NULL;
  }
  return eq_type->value.app.func->value.app.arg;
}

// Returns the rhs of an equality The equality expresison is an opaque
// reference, and exist in the given context. If the context contains
// eq_expression, then its type should be "(eq lhs) rhs", and this function
// returns rhs. If any steps fails, return NULL.
Expression *get_rhs_eq(Context *context, Expression *eq_expression) {
  Expression *eq_type = context_lookup(context, eq_expression);
  if (eq_type == NULL) {
    return NULL;
  }
  if (eq_type->type != APP_EXPRESSION) {
    return NULL;
  }
  if (eq_type->value.app.func->type != APP_EXPRESSION) {
    return NULL;
  }
  Expression *expected_eq = eq_type->value.app.func->value.app.func->value.app.func;
  if (expected_eq != eq) {
    return NULL;
  }
  return eq_type->value.app.arg;
}

RewriteProof *rewrite_head(Expression *expr, Expression *lemma) {
  Context *ctx = get_expression_context(expr);
  Expression *lhs = get_lhs_eq(ctx, lemma);
  Expression *rhs = get_rhs_eq(ctx, lemma);
  if (expr_match(lhs, expr)) {
    RewriteProof *equality = init_rewrite_proof(expr, rhs, lemma);
    return equality;
  } else {
    return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
  }
}

RewriteProof *rewrite_app(Context *ctx, Expression *expr, Expression *lemma) {
  Context *app_context = get_expression_context(expr);

  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = rewrite(ctx, func, lemma);
  RewriteProof *rw_arg_proof = rewrite(ctx, arg, lemma);

  // Expression *mid;
  // RewriteProof *fx_mid;
  RewriteProof *mid_rewrite_proof;

  if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
    mid_rewrite_proof = init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
  } else {
    mid_rewrite_proof = init_rewrite_proof(
        expr,
        init_app_expression(expr->value.app.context,
                            rw_func_proof->rewritten_expr,
                            rw_arg_proof->rewritten_expr),
        build_app_cong(app_context, rw_func_proof, rw_arg_proof));
  }

  Expression *mid = mid_rewrite_proof->rewritten_expr;
  Expression *fx_mid = mid_rewrite_proof->equality_proof;

  RewriteProof *rewritten_mid = rewrite_head(mid, lemma);
  if (nothing_rewritten(rewritten_mid)) {
    free_rewrite_proof(rewritten_mid);
    return init_rewrite_proof(expr, mid, fx_mid);
  } else {
    return init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(app_context, mid_rewrite_proof, rewritten_mid));
  }
}

// Attempt an aggressive rewrite in expr using the provided lemma. The lemma
// should be an opaque reference that is valid in the context.
RewriteProof *rewrite(Context *ctx, Expression *expr, Expression *lemma) {
  switch (expr->type) {
    case (APP_EXPRESSION):
      return rewrite_app(ctx, expr, lemma);
    case (VAR_EXPRESSION):
      return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
    default:
      return NULL;  // TODO: Unsupported.
  }
}