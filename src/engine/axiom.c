#include "axiom.h"

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *f_equal = NULL;
Expression *eq_trans = NULL;
Expression *f = NULL;
Expression *g = NULL;
Expression *a = NULL;
Expression *eq_fa_a = NULL;
Expression *nat = NULL;
Context *f_a_ctx = NULL;
Context *g_f_a_ctx = NULL;


// Takes in any context, and extends it with nat.
Context *extend_with_nat(Context *ctx) {
  return context_insert(ctx, nat, init_type_expression());
}

// Takes in a context in which nat is defined, and extends it with eq, eq_refl.
Context *extend_with_eq(Context *ctx) {
  // defining the eq type
  Expression *type = init_type_expression();
  Expression *eq_ty =
      init_arrow_expression(nat, init_arrow_expression(nat, type));
  Context *new_ctx = context_insert(ctx, eq, eq_ty);

  // defining the refl type
  Expression *x = init_var_expression("x");
  Context *new_ctx_with_x = context_insert(new_ctx, x, nat);
  Expression *refl_ty = init_forall_expression(
      new_ctx_with_x,
      init_app_expression(new_ctx_with_x,
                          init_app_expression(new_ctx_with_x, eq, x), x));

  return context_insert(new_ctx, eq_refl, refl_ty);
}

// Takes in a context which has nat and eq defined within it, and extends it
// with f_equal.
Context *extend_with_f_equal(Context *ctx) {
  // defining the f_equal type
  Expression *y = init_var_expression("y");
  Expression *x = init_var_expression("x");
  Expression *f = init_var_expression("f");

  Context *f_equal_ctx = context_create_empty();
  f_equal_ctx = extend_with_eq(extend_with_nat(f_equal_ctx));
  Context *y_ctx = context_insert(f_equal_ctx, y, nat);
  Context *x_ctx = context_insert(y_ctx, x, nat);
  f_equal_ctx = context_insert(x_ctx, f, init_arrow_expression(nat, nat));

  Expression *f_equal_H = init_app_expression(
      f_equal_ctx, init_app_expression(f_equal_ctx, eq, x), y);
  Expression *f_x = init_app_expression(f_equal_ctx, f, x);
  Expression *f_y = init_app_expression(f_equal_ctx, f, y);

  Expression *f_equal_concl = init_app_expression(
      f_equal_ctx, init_app_expression(f_equal_ctx, eq, f_x), f_y);
  Expression *f_equal_body = init_arrow_expression(f_equal_H, f_equal_concl);

  Expression *f_equal_ty = init_forall_expression(
      f_equal_ctx, init_forall_expression(
                       x_ctx, init_forall_expression(y_ctx, f_equal_body)));
  return context_insert(ctx, f_equal, f_equal_ty);
}

// Takes in a context which has nat and eq defined within it, and extends it
// with eq_trans
Context *extend_with_eq_trans(Context *ctx) {
  // defining the eq_trans type
  Expression *z = init_var_expression("z");
  Expression *y = init_var_expression("y");
  Expression *x = init_var_expression("x");

  Context *eq_trans_ctx = context_create_empty();
  eq_trans_ctx = extend_with_eq(extend_with_nat(eq_trans_ctx));
  Context *z_ctx = context_insert(eq_trans_ctx, z, nat);
  Context *y_ctx = context_insert(z_ctx, y, nat);
  eq_trans_ctx = context_insert(y_ctx, x, nat);

  Expression *eq_trans_H1 = init_app_expression(
      eq_trans_ctx, init_app_expression(eq_trans_ctx, eq, x), y);
  Expression *eq_trans_H2 = init_app_expression(
      eq_trans_ctx, init_app_expression(eq_trans_ctx, eq, y), z);
  Expression *eq_trans_concl = init_app_expression(
      eq_trans_ctx, init_app_expression(eq_trans_ctx, eq, x), z);

  Expression *eq_trans_body = init_arrow_expression(
      eq_trans_H1, init_arrow_expression(eq_trans_H2, eq_trans_concl));

  Expression *eq_trans_ty = init_forall_expression(
      eq_trans_ctx, init_forall_expression(
                        y_ctx, init_forall_expression(z_ctx, eq_trans_body)));
  return context_insert(ctx, eq_trans, eq_trans_ty);
}

void init_globals() {
  if (!eq) eq = init_var_expression("eq");
  if (!eq_refl) eq_refl = init_var_expression("eq_refl");
  if (!f_equal) f_equal = init_var_expression("f_equal");
  if (!eq_trans) eq_trans = init_var_expression("eq_trans");
  if (!f) f = init_var_expression("f");
  if (!g) g = init_var_expression("g");
  if (!a) a = init_var_expression("a");
  if (!eq_fa_a) eq_fa_a = init_var_expression("eq_fa_a");
  if (!nat) nat = init_var_expression("nat");

  if (!f_a_ctx) {
    f_a_ctx = context_create_empty();
    f_a_ctx = extend_with_nat(f_a_ctx);
    f_a_ctx = extend_with_eq(f_a_ctx);
    f_a_ctx = extend_with_f_equal(f_a_ctx);
    f_a_ctx = extend_with_eq_trans(f_a_ctx);

    Expression *f_ty = init_arrow_expression(nat, nat);
    Expression *a_ty = nat;

    f_a_ctx = context_insert(f_a_ctx, f, f_ty);
    f_a_ctx = context_insert(f_a_ctx, a, a_ty);

    Expression *eq_fa_a_ty = init_app_expression(
        f_a_ctx,
        init_app_expression(f_a_ctx, eq, init_app_expression(f_a_ctx, f, a)),
        a);
    f_a_ctx = context_insert(f_a_ctx, eq_fa_a, eq_fa_a_ty);
  };

  if (!g_f_a_ctx) {
    Expression *g_ty = init_arrow_expression(nat, nat);
    g_f_a_ctx = context_insert(f_a_ctx, g, g_ty);
  };
}

Expression *build_fa() {
  init_globals();
  Expression *fa = init_app_expression(f_a_ctx, f, a);
  return fa;
}

Expression *compute_f_a(Expression *e) {
  if (e->type == APP_EXPRESSION && e->value.app.func == f) {
    if (e->value.app.arg == a) {
      return a;
    }
    return compute_f_a(e->value.app.arg);
  }
  return e;
}

bool equivalent_under_computation(Expression *a, Expression *b) {
  if (a == b) {
    return true;
  }

  if (a->type != b->type) {
    return false;
  }

  switch (a->type) {
    case (TYPE_EXPRESSION):
      return true;
    case (APP_EXPRESSION):
      return equivalent_under_computation(compute_f_a(a->value.app.func),
                                          compute_f_a(b->value.app.func)) &&
             equivalent_under_computation(compute_f_a(a->value.app.arg),
                                          compute_f_a(b->value.app.arg));
    case (FORALL_EXPRESSION):
      return equivalent_under_computation(
          a->value.forall.body,
          b->value.forall.body);  // TODO: Needs to be tigher
    default:
      return false;  // do more later
  }
}