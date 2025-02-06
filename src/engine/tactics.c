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

RewriteProof *rewrite_head(Expression *expr, Expression *lemma) {
  Context *e_ctx = get_expression_context(expr);
  Expression *lemma_ty = get_expression_type(lemma);

  if (lemma_ty->type == FORALL_EXPRESSION) {
    Expression *instantiated_lemma = unify_and_instantiate(e_ctx, lemma, lemma_ty, expr);
    if (instantiated_lemma == NULL) {
      return NULL;
    }

    Expression *lhs = get_lhs_eq(get_expression_type(instantiated_lemma));
    Expression *rhs = get_rhs_eq(get_expression_type(instantiated_lemma));
    if (expr_match(lhs, expr)) {
      return init_rewrite_proof(expr, rhs, instantiated_lemma);
    } else {
      return init_rewrite_proof(expr, expr, build_eq_refl(expr));
    }
  }

  Expression *lhs = get_lhs_eq(lemma_ty);
  Expression *rhs = get_rhs_eq(lemma_ty);
  if (expr_match(lhs, expr)) {
    return init_rewrite_proof(expr, rhs, lemma);
  } else {
    return init_rewrite_proof(expr, expr, build_eq_refl(expr));
  }
}

// Given an expr := lambda x: T, B, returns the expression fun x': T, B where x'
// has substituted all appearances of x in B and x' is a fresh variable.
Expression *replace_with_fresh_lambda(Expression *expr) {
  Expression *bound_x = expr->value.lambda.bound_variable;
  Expression *bound_x_ty = bound_x->value.var.type;

  char *xp_name = strcat(strdup(bound_x->value.var.name), "'");
  Expression *xp = init_var_expression(xp_name, bound_x_ty);

  Expression *beta_reduced = reduce(expr, xp);
  Expression *fresh = init_lambda_expression(xp, beta_reduced);
  return fresh;
}

// Take a lambda expression as input, like fun x: T => B, and
// return a new lambda expression with a fresh variable x' and
// x substituted for by x'.
Expression *refresh(Expression *expr) {
  if (expr->type != LAMBDA_EXPRESSION) {
    return NULL;
  }

  Expression *x = expr->value.lambda.bound_variable;
  char *x_name = x->value.var.name;
  Expression *T = get_expression_type(x);
  Expression *B = expr->value.lambda.body;

  char *xp_name = strcat(strdup(x_name), "'");
  Expression *xp = init_var_expression(xp_name, T);
  return init_lambda_expression(xp, subst(B, x, xp));
}

RewriteProof *rewrite_lambda(Expression *expr, Expression *lemma) {
  Expression *x = expr->value.lambda.bound_variable;
  Expression *T = get_expression_type(x);
  Expression *inner_orig = expr->value.lambda.body;

  RewriteProof *inner_rw = rewrite(inner_orig, lemma);
  Expression *mid = refresh(init_lambda_expression(x, inner_rw->rewritten_expr));

  Expression *eq_pf_ty = get_expression_type(inner_rw->equality_proof);
  Expression *pre_func_ext = refresh(init_lambda_expression(x, inner_rw->equality_proof));

  Expression *A = T;
  Expression *B = get_expression_type(inner_orig);

  Expression *eq_ty_lhs = refresh(init_lambda_expression(x, get_lhs_eq(eq_pf_ty)));
  Expression *eq_ty_rhs = refresh(init_lambda_expression(x, get_rhs_eq(eq_pf_ty)));

  // @functional_extensionality : forall (A B : Type) (f g : A -> B), (forall x
  // : A, eq B (f x) (g x)) -> eq (A -> B) f g
  Expression *f_mid = build_lambda_extensionality(A, B, eq_ty_lhs, eq_ty_rhs, pre_func_ext);
  RewriteProof *rewritten_mid = rewrite_head(mid, lemma);

  if (nothing_rewritten(rewritten_mid)) {
    free_rewrite_proof(rewritten_mid);
    return init_rewrite_proof(expr, mid, f_mid);
  } else {
    return init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(init_rewrite_proof(expr, mid, f_mid), rewritten_mid));
  }
}

RewriteProof *rewrite_app(Expression *expr, Expression *lemma) {
  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = rewrite(func, lemma);
  RewriteProof *rw_arg_proof = rewrite(arg, lemma);

  RewriteProof *mid_rewrite_proof;

  if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
    mid_rewrite_proof =
        init_rewrite_proof(expr, expr, build_eq_refl(expr));
  } else {
    mid_rewrite_proof = init_rewrite_proof(
        expr,
        init_app_expression(rw_func_proof->rewritten_expr,
                            rw_arg_proof->rewritten_expr),
        build_app_cong(rw_func_proof, rw_arg_proof));
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
        build_eq_trans(mid_rewrite_proof, rewritten_mid));
  }
}

RewriteProof *rewrite_var(Expression *expr, Expression *lemma) {
  RewriteProof *rewritten_expr = rewrite_head(expr, lemma);
  if (nothing_rewritten(rewritten_expr)) {
    return init_rewrite_proof(expr, expr, build_eq_refl(expr));
  } else {
    return rewritten_expr;
  }
}

// Attempt an aggressive rewrite in expr using the provided lemma. The lemma
// should be an opaque reference that is valid in the context.
RewriteProof *rewrite(Expression *expr, Expression *lemma) {
  switch (expr->type) {
    case (APP_EXPRESSION):
      return rewrite_app(expr, lemma);
    case (LAMBDA_EXPRESSION):
      return rewrite_lambda(expr, lemma);
    case (VAR_EXPRESSION):
      return rewrite_var(expr, lemma);
    default:
      return NULL;  // TODO: Unsupported.
  }
}