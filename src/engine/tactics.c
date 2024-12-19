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
  Context *ctx = get_expression_context(expr);
  Expression *lemma_ty = context_lookup(ctx, lemma);

  if (lemma_ty->type == FORALL_EXPRESSION) {
    // Need to run unification.
    Map *binding_results = unify(ctx, lemma_ty, expr);
    if (binding_results == NULL) {
      return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
    }
    Expression *proof =
        instantiate_lemma_with_bindings(ctx, lemma, lemma_ty, binding_results);
    Expression *lhs = get_lhs_eq(get_expression_type(ctx, proof));
    Expression *rhs = get_rhs_eq(get_expression_type(ctx, proof));
    if (expr_match(lhs, expr)) {
      return init_rewrite_proof(expr, rhs, proof);
    } else {
      return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
    }
  }

  Expression *lhs = get_lhs_eq(lemma_ty);
  Expression *rhs = get_rhs_eq(lemma_ty);
  if (expr_match(lhs, expr)) {
    return init_rewrite_proof(expr, rhs, lemma);
  } else {
    return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
  }
}

// Given an expr := lambda x: T, B, returns the expression fun x': T, B where x'
// has substituted all appearances of x in B and x' is a fresh variable.
Expression *replace_with_fresh_lambda(Expression *expr) {
  Context *bound_x = expr->value.lambda.bound_variable;
  Expression *x = get_binding_variable(bound_x);
  Expression *T = get_binding_variable_type(bound_x);

  char *xp_name = strcat(strdup(x->value.var.name), "'");
  Expression *xp = init_var_expression(xp_name);
  Context *par = bound_x->parent;
  Context *bound_xp = context_insert(par, xp, T);

  Expression *beta_reduced = reduce(expr, xp);
  Expression *fresh = init_lambda_expression(bound_xp, beta_reduced);
  return fresh;
}

RewriteProof *rewrite_lambda(Context *ctx, Expression *expr,
                             Expression *lemma) {
  Context *lambda_context = get_expression_context(expr);

  Context *bound_x = expr->value.lambda.bound_variable;
  Expression *x = get_binding_variable(bound_x);
  Expression *T = get_binding_variable_type(bound_x);
  Expression *inner_orig = expr->value.lambda.body;

  RewriteProof *inner_rw = rewrite(ctx, inner_orig, lemma);
  Expression *rrw = init_lambda_expression(bound_x, inner_rw->rewritten_expr);
  Expression *mid = replace_with_fresh_lambda(rrw);

  Expression *eq_pf_ty = get_expression_type(ctx, inner_rw->equality_proof);
  Expression *rrw_pf = init_lambda_expression(bound_x, inner_rw->equality_proof);
  Expression *pre_func_ext = replace_with_fresh_lambda(rrw_pf);

  Expression *A = T;
  Expression *B = get_expression_type(ctx, inner_orig); // ?

  Expression *eq_ty_lhs = replace_with_fresh_lambda(init_lambda_expression(bound_x, get_lhs_eq(eq_pf_ty)));
  Expression *eq_ty_rhs = replace_with_fresh_lambda(init_lambda_expression(bound_x, get_rhs_eq(eq_pf_ty)));

  Expression *f_mid = build_lambda_extensionality(lambda_context, A, B, eq_ty_lhs, eq_ty_rhs, pre_func_ext);


  // @functional_extensionality : forall (A B : Type) (f g : A -> B), (forall x
  // : A, eq B (f x) (g x)) -> eq (A -> B) f g
  RewriteProof *rewritten_mid = rewrite_head(mid, lemma);

  if (nothing_rewritten(rewritten_mid)) {
    free_rewrite_proof(rewritten_mid);
    return init_rewrite_proof(expr, mid, f_mid);
  } else {
    return init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(lambda_context, init_rewrite_proof(expr, mid, f_mid),
                       rewritten_mid));
  }
}

RewriteProof *rewrite_app(Context *ctx, Expression *expr, Expression *lemma) {
  Context *app_context = get_expression_context(expr);

  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = rewrite(ctx, func, lemma);
  RewriteProof *rw_arg_proof = rewrite(ctx, arg, lemma);

  RewriteProof *mid_rewrite_proof;

  if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
    mid_rewrite_proof =
        init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
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
    case (LAMBDA_EXPRESSION):
      return rewrite_lambda(ctx, expr, lemma);
    case (VAR_EXPRESSION):
      return init_rewrite_proof(expr, expr, build_eq_refl(ctx, expr));
    default:
      return NULL;  // TODO: Unsupported.
  }
}