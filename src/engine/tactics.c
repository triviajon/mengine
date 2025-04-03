#include "tactics.h"

static int rewrite_cache_hits = 0;
static int rewrite_locations = 0;

bool nothing_rewritten(RewriteProof *rewrite_proof) {
  return rewrite_proof->expr == rewrite_proof->rewritten_expr;
}

bool rewrite_failed(RewriteProof *rewrite_proof) {
  return rewrite_proof == NULL;
}

RewriteProof *get_rresult(Expression *expr) {
  switch (expr->type) {
    case (APP_EXPRESSION): {
      return expr->value.app.rresult;
    }
    case (LAMBDA_EXPRESSION): {
      return expr->value.lambda.rresult;
    }
    case (VAR_EXPRESSION): {
      return expr->value.var.rresult;
    }
    default: 
      return NULL;
  }
}

void set_rresult(Expression *expr, RewriteProof *rresult) {
  switch (expr->type) {
    case (APP_EXPRESSION): {
      expr->value.app.rresult = rresult; break;
    }
    case (LAMBDA_EXPRESSION): {
      expr->value.lambda.rresult = rresult; break;
    }
    case (VAR_EXPRESSION): {
      expr->value.var.rresult = rresult; break;
    }
    default: 
      break;
  }
}


bool expr_match(Expression *expr1, Expression *expr2) {
  if (expr1 == expr2) {
    return true;
  }

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
  Expression *lemma_ty = get_expression_type(lemma);

  if (lemma_ty->type == FORALL_EXPRESSION) {
    Expression *instantiated_lemma = unify_and_instantiate(lemma, lemma_ty, expr);
    if (instantiated_lemma != NULL) {
      Expression *lhs = get_lhs_eq(get_expression_type(instantiated_lemma));
      Expression *rhs = get_rhs_eq(get_expression_type(instantiated_lemma));

      if (expr_match(lhs, expr)) {
        rewrite_locations++;
        return init_rewrite_proof(expr, rhs, instantiated_lemma);
      }
    }

    return init_rewrite_proof(expr, expr, build_eq_refl(expr));
  }

  Expression *lhs = get_lhs_eq(lemma_ty);
  Expression *rhs = get_rhs_eq(lemma_ty);
  if (expr_match(lhs, expr)) {
    rewrite_locations++;
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

Expression *refresh2(Expression *expr, char suffix, bool add_suffix) {
  if (expr->type != LAMBDA_EXPRESSION) {
    return NULL;
  }

  Expression *x = expr->value.lambda.bound_variable;
  char *x_name = x->value.var.name;
  Expression *T = get_expression_type(x);
  Expression *B = expr->value.lambda.body;

  size_t len = strlen(x_name);
  char *xp_name = (char *)malloc(len + (add_suffix ? 2 : 1));
  strcpy(xp_name, x_name);
  if (add_suffix) xp_name[len] = suffix;
  xp_name[len + (add_suffix ? 1 : 0)] = '\0';

  Expression *xp = init_var_expression(xp_name, T);
  free(xp_name);
  return init_lambda_expression(xp, subst(B, x, xp));
}


RewriteProof *rewrite_lambda(Expression *expr, Expression *lemma) {
  Expression *x = expr->value.lambda.bound_variable;
  Expression *T = get_expression_type(x);
  Expression *inner_orig = expr->value.lambda.body;

  RewriteProof *inner_rw = _rewrite(inner_orig, lemma);
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

  RewriteProof *result;
  if (nothing_rewritten(rewritten_mid)) {
    result = init_rewrite_proof(expr, mid, f_mid);
  } else {
    result = init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(init_rewrite_proof(expr, mid, f_mid), rewritten_mid));
  }

  free_rewrite_proof(rewritten_mid);
  set_rresult(expr, result);
  return result;
}

RewriteProof *rewrite_app(Expression *expr, Expression *lemma) {
  Expression *func = expr->value.app.func;
  Expression *arg = expr->value.app.arg;
  RewriteProof *rw_func_proof = _rewrite(func, lemma);
  RewriteProof *rw_arg_proof = _rewrite(arg, lemma);

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
  RewriteProof *result;
  if (nothing_rewritten(rewritten_mid)) {
    result = init_rewrite_proof(expr, mid, fx_mid);
  } else {
    result = init_rewrite_proof(
        expr, rewritten_mid->rewritten_expr,
        build_eq_trans(mid_rewrite_proof, rewritten_mid));
  }

  free_rewrite_proof(mid_rewrite_proof);
  free_rewrite_proof(rewritten_mid);

  set_rresult(expr, result);
  return result;
}

RewriteProof *rewrite_var(Expression *expr, Expression *lemma) {
  RewriteProof *rewritten_expr = rewrite_head(expr, lemma);
  if (nothing_rewritten(rewritten_expr)) {
    RewriteProof *result = init_rewrite_proof(expr, expr, build_eq_refl(expr));
    set_rresult(expr, result);
    return result;
  } else {
    set_rresult(expr, rewritten_expr);
    return rewritten_expr;
  }
}

RewriteProof *rewrite_hole(Expression *expr) {
  return init_rewrite_proof(expr, expr, build_eq_refl(expr));
}

int get_rewrite_cache_hits() {
  return rewrite_cache_hits;
}

int get_rewrite_locations() {
  return rewrite_locations;
}

// Internal rewriting function.
RewriteProof *_rewrite(Expression *expr, Expression *lemma) {
  RewriteProof *cached_result = get_rresult(expr);
  if (cached_result != NULL) {
    rewrite_cache_hits++;
    return cached_result;
  }

  switch (expr->type) {
    case (APP_EXPRESSION):
      return rewrite_app(expr, lemma);
    case (LAMBDA_EXPRESSION):
      return rewrite_lambda(expr, lemma);
    case (VAR_EXPRESSION):
      return rewrite_var(expr, lemma);
    case (HOLE_EXPRESSION):
      return rewrite_hole(expr);
    default:
      return NULL;  // TODO: Unsupported.
  }
}

void clear_rewrite_proofs(Expression *expr) {
  // Need to double check for correctness.

  switch (expr->type) {
    case (APP_EXPRESSION): {
      if (expr->value.app.rresult == NULL) {
        break;
      }
      expr->value.app.rresult = NULL;
      clear_rewrite_proofs(expr->value.app.func);
      clear_rewrite_proofs(expr->value.app.arg);
      break;
    }
    case (LAMBDA_EXPRESSION): {
      if (expr->value.lambda.rresult == NULL) {
        break;
      }
      expr->value.lambda.rresult = NULL;
      clear_rewrite_proofs(expr->value.lambda.body);
      break;
    }
    case (VAR_EXPRESSION): {
      expr->value.var.rresult = NULL;
      break;
    }
    default: 
      break;
  }
}

// Top level rewriting function. Is responsible for clearing the cached
// RewriteProof results of Expressions.
RewriteProof *rewrite(Expression *expr, Expression *lemma) {
  RewriteProof *result = NULL;
  switch (expr->type) {
    case (APP_EXPRESSION):
      result = rewrite_app(expr, lemma); break;
    case (LAMBDA_EXPRESSION):
      result = rewrite_lambda(expr, lemma); break;
    case (VAR_EXPRESSION):
      result = rewrite_var(expr, lemma); break;
    default:
      return NULL;  // TODO: Unsupported.
  }

  clear_rewrite_proofs(expr);
  return result;
}

// Given a goal (a hole with an expected return type) and a lemma, 
// this function attempts to transform the goal's return type according to the lemma.
// I.e., on a successful rewrite of the goal's type, this function will return two things:
// 	1) A new hole, representing the new goal to find a term which has it's return type.
//  2) A proof term that satisfies the original goal. This proof term will have the above mentioned hole,
//     which will need to be filled.
RewrittenGoal *rewrite_transform(Expression *goal, Expression *rewrite_lemma) {
  Expression *goal_type = get_expression_type(goal);
  RewriteProof *rewrite_proof = rewrite(goal_type, rewrite_lemma);
  
  Expression *new_goal = init_hole_expression("Goal", rewrite_proof->rewritten_expr, get_expression_context(goal));
  Expression *proof = init_app_expression(init_app_expression(init_app_expression(init_app_expression(
		eq_subst, rewrite_proof->expr), rewrite_proof->rewritten_expr), rewrite_proof->equality_proof), new_goal);
  
  fillHole(goal, proof);

  return init_rewritten_goal(new_goal, proof);
}


DoublyLinkedList *apply(Expression *goal, Expression *lemma) {
  UnificationResult *unification_result = eunify(lemma, goal);
  Expression *instantiated_lemma = unification_result->lemma_instantiation;
  DoublyLinkedList *new_goals = unification_result->new_goals;
  if (can_fill(goal, instantiated_lemma)) {
    fillHole(goal, instantiated_lemma);
    return new_goals;
  }
  return NULL;
}

DoublyLinkedList *eapply(Expression *goal, Expression *lemma) {
  UnificationResult *unification_result = eunify(lemma, goal);
  Expression *instantiated_lemma = unification_result->lemma_instantiation;
  DoublyLinkedList *new_goals = unification_result->new_goals;
  fillHole(goal, instantiated_lemma);
  return new_goals;
}

IntroReturn *intro(Expression *goal) {
  if (goal->type != HOLE_EXPRESSION) return NULL;

  Expression *goal_ty = get_expression_type(goal);
  if (goal_ty->type != FORALL_EXPRESSION) return NULL;

  Expression *goal_ty_bv = goal_ty->value.forall.bound_variable;
  Expression *goal_ty_body = goal_ty->value.forall.body;

  Context *new_context = context_insert(get_expression_context(goal), goal_ty_bv);

  Expression *new_goal = init_hole_expression("Goal", goal_ty_body, new_context);
  Expression *proof_of_original = init_lambda_expression(goal_ty_bv, new_goal);

  if (can_fill(goal, proof_of_original)) {
    fillHole(goal, proof_of_original);
    return init_intro_return(goal, new_goal, proof_of_original);
  }
  return NULL;
}