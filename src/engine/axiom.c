#include "axiom.h"

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *f_equal = NULL;
Expression *eq_trans = NULL;
Expression *f = NULL;
Expression *g = NULL;
Expression *h = NULL;
Expression *a = NULL;
Expression *eq_fa_a = NULL;
Expression *eq_haa_a = NULL;
Expression *nat = NULL;
Context *f_a_ctx = NULL;
Context *g_f_a_ctx = NULL;
Context *h_g_f_a_ctx = NULL;

// Takes in any context, and extends it with nat.
Context *extend_with_nat(Context *ctx) {
  return context_insert(ctx, nat, init_type_expression());
}

// Takes in a context in which nat is defined, and extends it with eq, eq_refl.
Context *extend_with_eq(Context *ctx) {
  // defining the eq type. eq : forall A : Type, A -> A -> Prop
  Expression *prop = init_prop_expression();
  Expression *type = init_type_expression();
  Expression *A = init_var_expression("A");
  Context *eq_ty_ctx = context_insert(context_create_empty(), A, type); 
  Expression *eq_ty = init_forall_expression(eq_ty_ctx, init_arrow_expression(eq_ty_ctx, A, init_arrow_expression(eq_ty_ctx, A, prop)));
  Context *ctx_with_eq = context_insert(ctx, eq, eq_ty);

  // defining the refl type. eq_refl : forall (B : Type) (x : B), eq B x x. 
  Expression *B = init_var_expression("B");
  Expression *x = init_var_expression("x");
  Context *base_eq_ctx = context_insert(context_create_empty(), eq, eq_ty);
  Context *eq_with_B_ctx = context_insert(base_eq_ctx, B, type);
  Context *B_with_x_ctx = context_insert(eq_with_B_ctx, x, B);
  Expression *refl_ty = init_forall_expression(eq_with_B_ctx, 
    init_forall_expression(B_with_x_ctx, 
      init_app_expression(B_with_x_ctx, 
        init_app_expression(B_with_x_ctx, 
          init_app_expression(B_with_x_ctx, eq, B), x), x)));

  Context *ctx_with_eq_and_refl = context_insert(ctx_with_eq, eq_refl, refl_ty);

  return ctx_with_eq_and_refl;
}

// Takes in a context which has nat and eq defined within it, and extends it
// with f_equal.
Context *extend_with_f_equal(Context *ctx) {
  // defining the f_equal type
  Expression *x = init_var_expression("x");
  Expression *y = init_var_expression("y");
  Expression *f = init_var_expression("f");

  Context *with_nat = extend_with_nat(context_create_empty());
  Context *base_ctx = extend_with_eq(with_nat);
  Context *f_ctx = context_insert(base_ctx, f, init_arrow_expression(with_nat, nat, nat));
  Context *x_ctx = context_insert(f_ctx, x, nat);
  Context *full_ctx = context_insert(x_ctx, y, nat);

  Expression *f_equal_H = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, nat), x), y);
  Expression *f_x = init_app_expression(full_ctx, f, x);
  Expression *f_y = init_app_expression(full_ctx, f, y);

  Expression *f_equal_concl = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, nat), f_x), f_y);
  Expression *f_equal_body = init_arrow_expression(full_ctx, f_equal_H, f_equal_concl);

  Expression *f_equal_ty = init_forall_expression(f_ctx, 
    init_forall_expression(x_ctx, 
      init_forall_expression(full_ctx, f_equal_body)));
  return context_insert(ctx, f_equal, f_equal_ty);
}

// Takes in a context which has nat and eq defined within it, and extends it
// with eq_trans
Context *extend_with_eq_trans(Context *ctx) {
  // defining the eq_trans type
  Expression *x = init_var_expression("x");
  Expression *y = init_var_expression("y");
  Expression *z = init_var_expression("z");

  Context *base_ctx = extend_with_eq(extend_with_nat(context_create_empty()));
  Context *x_ctx = context_insert(base_ctx, x, nat);
  Context *y_ctx = context_insert(x_ctx, y, nat);
  Context *full_ctx = context_insert(y_ctx, z, nat);

  Expression *eq_trans_H1 = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, nat), x), y);
  Expression *eq_trans_H2 = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, nat), y), z);
  Expression *eq_trans_concl = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, nat), x), z);

  Expression *eq_trans_body = init_arrow_expression(full_ctx, eq_trans_H1, 
    init_arrow_expression(full_ctx, eq_trans_H2, eq_trans_concl));

  Expression *eq_trans_ty = init_forall_expression(x_ctx, 
    init_forall_expression(y_ctx, 
      init_forall_expression(full_ctx, eq_trans_body)));
  return context_insert(ctx, eq_trans, eq_trans_ty);
}

void init_globals() {
  if (!eq) eq = init_var_expression("eq");
  if (!eq_refl) eq_refl = init_var_expression("eq_refl");
  if (!f_equal) f_equal = init_var_expression("f_equal");
  if (!eq_trans) eq_trans = init_var_expression("eq_trans");
  if (!f) f = init_var_expression("f");
  if (!g) g = init_var_expression("g");
  if (!h) h = init_var_expression("h");
  if (!a) a = init_var_expression("a");
  if (!eq_fa_a) eq_fa_a = init_var_expression("eq_fa_a");
  if (!eq_haa_a) eq_haa_a = init_var_expression("eq_haa_a");
  if (!nat) nat = init_var_expression("nat");

  if (!f_a_ctx) {
    f_a_ctx = context_create_empty();
    f_a_ctx = extend_with_nat(f_a_ctx);
    f_a_ctx = extend_with_eq(f_a_ctx);
    f_a_ctx = extend_with_f_equal(f_a_ctx);
    f_a_ctx = extend_with_eq_trans(f_a_ctx);

    Expression *f_ty = init_arrow_expression(f_a_ctx, nat, nat);
    Expression *a_ty = nat;

    f_a_ctx = context_insert(f_a_ctx, f, f_ty);
    f_a_ctx = context_insert(f_a_ctx, a, a_ty);

    Expression *eq_fa_a_ty = init_app_expression(
        f_a_ctx, init_app_expression(f_a_ctx, 
          init_app_expression(f_a_ctx, eq, nat), init_app_expression(f_a_ctx, f, a)), a);
    f_a_ctx = context_insert(f_a_ctx, eq_fa_a, eq_fa_a_ty);
  };

  if (!g_f_a_ctx) {
    Expression *g_ty = init_arrow_expression(nat, nat, nat);
    g_f_a_ctx = context_insert(f_a_ctx, g, g_ty);
  };

  if (!h_g_f_a_ctx) {
    Expression *h_ty = init_arrow_expression(f_a_ctx, nat, init_arrow_expression(f_a_ctx, nat, nat));
    h_g_f_a_ctx = context_insert(g_f_a_ctx, h, h_ty);

    Expression *haa = init_app_expression(h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, a), a);

    Expression *eq_haa_a_ty = init_app_expression(h_g_f_a_ctx, 
      init_app_expression(h_g_f_a_ctx, 
        init_app_expression(h_g_f_a_ctx, eq, nat), haa), a);

    h_g_f_a_ctx = context_insert(h_g_f_a_ctx, eq_haa_a, eq_haa_a_ty);
  }
}

Expression *build_fa() {
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

  if (a->type == PROP_EXPRESSION && b->type == TYPE_EXPRESSION) {
    return true; // TODO: A hint of subtyping...
  }

  if (a->type != b->type) {
    return false;
  }

  switch (a->type) {
    case (TYPE_EXPRESSION):
    case (PROP_EXPRESSION):
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