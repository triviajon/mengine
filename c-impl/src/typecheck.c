#include "typecheck.h"

bool alpha_equivalent(Env *env, Expression *t1, Expression *t2) {
  if (t1->type != t2->type) {
    return false;
  }

  switch (t1->type) {
  case TYPE_EXPRESSION:
    return true;

  case VAR_EXPRESSION: {
    if (equal(t1, t2)) {
      return true;
    }
    return strcmp(env_lookup(env, t1->value.var.name),
                  env_lookup(env, t2->value.var.name)) == 0;
  }

  case LAMBDA_EXPRESSION:
  case FORALL_EXPRESSION: {
    char *name1 = t1->value.lambda.var->value.var.name;
    char *name2 = t2->value.lambda.var->value.var.name;

    env_insert(env, name1, name2);

    return alpha_equivalent(env, t1->value.lambda.type,
                            t2->value.lambda.type) &&
           alpha_equivalent(env, t1->value.lambda.body, t2->value.lambda.body);
  }

  case APP_EXPRESSION:
    return alpha_equivalent(env, t1->value.app.func, t2->value.app.func) &&
           alpha_equivalent(env, t1->value.app.arg, t2->value.app.arg);

  default:
    return false;
  }
}

// In expr, replace instances of var by subst.
Expression *substitute(Expression *expr, char *var, Expression *subst) {
  switch (expr->type) {
  case (VAR_EXPRESSION):
    if (strcmp(expr->value.var.name, var) == 0) {
      return subst;
    } else {
      return expr;
    }
  case (LAMBDA_EXPRESSION):
    if (strcmp(expr->value.lambda.var->value.var.name, var) == 0) {
      return expr;
    } else {
      return init_lambda_expression(
          expr->value.lambda.var,
          substitute(expr->value.lambda.type, var, subst),
          substitute(expr->value.lambda.body, var, subst));
    }
  case (APP_EXPRESSION):
    return init_app_expression(substitute(expr->value.app.func, var, subst),
                               substitute(expr->value.app.arg, var, subst));
  case (FORALL_EXPRESSION):
    return init_forall_expression(
        expr->value.forall.var, substitute(expr->value.forall.type, var, subst),
        substitute(expr->value.forall.arg, var, subst));
  case (TYPE_EXPRESSION):
    return init_type_expression();
  }
}

Step *substitute_rest_steps(Step *prog, char *var, Expression *subst) {
  switch (prog->type) {
  case LET_STEP:
    return init_let_step(prog->value.let.id,
                         substitute(prog->value.let.expr, var, subst),
                         substitute_rest_steps(prog->next, var, subst));
  case THEOREM_STEP: {
    Theorem *theorem = prog->value.theorem.theorem;
    Theorem *new_theorem =
        init_theorem(theorem->name, substitute(theorem->theorem, var, subst),
                     substitute(theorem->proof, var, subst));
    return init_theorem_step(new_theorem,
                             substitute_rest_steps(prog->next, var, subst));
  }
  case EXPR_STEP:
    return init_expr_step(substitute(prog->value.expr.expr, var, subst));
  case END_STEP:
    return prog;
  }
}

Expression *typecheck_lambda(Expression *gamma, Expression *var,
                             Expression *type, Expression *body) {
  Expression *new_gamma = set_in_context(gamma, var, type);
  Expression *body_type = typecheck_non_context(new_gamma, body);
  return init_forall_expression(var, type, body_type);
}

Expression *typecheck_app(Expression *gamma, Expression *func,
                          Expression *arg) {
  Expression *func_type = typecheck_non_context(gamma, func);
  Expression *arg_type = typecheck_non_context(gamma, arg);

  if (func_type->type == FORALL_EXPRESSION) {
    Env *env = env_init();
    if (alpha_equivalent(env, arg_type, func_type->value.forall.type)) {
      return substitute(func_type->value.forall.arg,
                        func_type->value.forall.var->value.var.name, arg);
    } else {
      // panic!
      return NULL;
    }
  } else {
    // panic!
    return NULL;
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
Expression *typecheck_forall(Expression *gamma, Expression *var,
                             Expression *type, Expression *arg) {
  Expression *new_gamma = set_in_context(gamma, var, type);
  if (is_valid_context(new_gamma)) {
    return init_type_expression();
  } else {
    // panic!
    return NULL;
  }
}
#pragma clang diagnostic pop

Expression *typecheck_expression_under_context(Expression *gamma,
                                               Expression *expr) {
  switch (expr->type) {
  case (VAR_EXPRESSION):
    return lookup_in_context(gamma, expr);
  case (LAMBDA_EXPRESSION):
    return typecheck_lambda(gamma, expr->value.lambda.var,
                            expr->value.lambda.type, expr->value.lambda.body);
  case (APP_EXPRESSION):
    return typecheck_app(gamma, expr->value.app.func, expr->value.app.arg);
  case (FORALL_EXPRESSION):
    return typecheck_forall(gamma, expr->value.forall.var,
                            expr->value.forall.type, expr->value.forall.arg);
  case (TYPE_EXPRESSION):
    return expr;
  }
}

// Returns true if delta is a valid context in the valid context gamma
bool typecheck_context(Expression *gamma, Expression *delta) {
  // Typecheck delta under gamma, as in
  // https://www.cs.cmu.edu/%7Efp/papers/mfps89.pdf
  switch (gamma->type) {
  case (TYPE_EXPRESSION):
    switch (delta->type) {
    case (TYPE_EXPRESSION):
      return true;
    case (FORALL_EXPRESSION): {
      Expression *new_gamma = set_in_context(gamma, delta->value.forall.var,
                                             delta->value.forall.type);
      return typecheck_context(new_gamma, delta->value.forall.arg);
    }
    default:
      return false;
    }
  case (FORALL_EXPRESSION):
    switch (delta->type) {
    case (TYPE_EXPRESSION): {
      Expression *delta_prime = top_context(gamma)->value.forall.type;
      Expression *gamma_prime = clear_top_context(gamma);
      if (is_valid_context(delta_prime)) {
        return typecheck_context(gamma_prime, delta_prime);
      } else {
        return typecheck_expression_under_context(gamma_prime, delta_prime)
                   ->type == TYPE_EXPRESSION;
      }
    }
    case (FORALL_EXPRESSION): {
      Expression *new_gamma = set_in_context(gamma, delta->value.forall.var,
                                             delta->value.forall.type);
      return typecheck_context(new_gamma, delta->value.forall.arg);
    }
    default:
      return false;
    }
  default: {
    switch (delta->type) {
    case (FORALL_EXPRESSION): {
      Expression *new_gamma = set_in_context(gamma, delta->value.forall.var,
                                             delta->value.forall.type);
      return typecheck_context(new_gamma, delta->value.forall.arg);
    }
    default:
      return false;
    }
  }
  }
}

Expression *typecheck_non_context(Expression *context, Expression *expr) {
  Expression *type = typecheck_expression_under_context(context, expr);
  return beta_reduction(context, type);
}

bool is_valid_context(Expression *expr) {
  switch (expr->type) {
  case TYPE_EXPRESSION:
    return true;
  case FORALL_EXPRESSION:
    return is_valid_context(expr->value.forall.arg);
  default:
    return false;
  }
}

Expression *typecheck_expression(Expression *expr) {
  if (is_valid_context(expr)) {
    Expression *context = init_type_expression();
    if (typecheck_context(expr, context)) {
      return init_type_expression();
    } else {
      // panic!
      return NULL;
    }
  } else {
    Expression *context = init_type_expression();
    return typecheck_non_context(context, expr);
  }
}

// Function to typecheck a Let statement
Step *typecheck_let(char *name, Expression *expr, Step *next) {
  Expression *expr_context = init_type_expression();
  Expression *expr_reduced = beta_reduction(expr_context, expr);
  printf("let %s := %s in.\n", name, stringify_expression(expr_reduced));
  Step *new_prog = substitute_rest_steps(next, name, expr_reduced);
  return new_prog;
}

// Function to typecheck a Theorem
Step *typecheck_theorem(Theorem *theorem, Step *next) {
  // To typecheck a theorem, we must make sure that it's
  // theorem term (type) is well-typed, and that it's proof is a
  // term of the theorem type

  typecheck_expression(theorem->theorem);

  Expression *theorem_context = init_type_expression();
  Expression *new_theorem_term = beta_reduction(theorem_context, theorem->theorem);
  Expression *proof_type = typecheck_expression(theorem->proof);
  Expression *proof_context = init_type_expression();
  Expression *proof_reduced = beta_reduction(proof_context, theorem->proof);
  Env *env = env_init();
  if (alpha_equivalent(env, new_theorem_term, proof_type)) {
    printf("Theorem %s : %s.\nProof: %s\n", theorem->name, stringify_expression(new_theorem_term), stringify_expression(theorem->proof));
    return substitute_rest_steps(next, theorem->name, proof_reduced);
  } else {
    // panic! proof type does not match theorem term
    printf("Proof type does not match theorem term!");
    return NULL;
  }
}

// Function to typecheck the entire program
void typecheck_prog(Step *step) {
  Step *current = step;

  while (current != NULL) {
    switch (current->type) {
    case END_STEP: {
        return;
      }
    case LET_STEP: {
        current = typecheck_let(current->value.let.id, current->value.let.expr, current->next);
        break;
      }
    case THEOREM_STEP: {
        current = typecheck_theorem(current->value.theorem.theorem, current->next);
        break;
      }
    case EXPR_STEP: {
        Expression *typed = typecheck_expression(current->value.expr.expr);
        printf("Expression: %s\n", stringify_expression(typed));
        current = current->next;
        break;
      } 
    }
  }
}