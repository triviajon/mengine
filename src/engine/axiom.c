#include "axiom.h"

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *app_cong = NULL;
Expression *eq_trans = NULL;
Expression *lambda_extensionality = NULL;
Expression *f = NULL;
Expression *g = NULL;
Expression *h = NULL;
Expression *a = NULL;
Expression *b = NULL;
Expression *c = NULL;
Expression *eq_fa_a = NULL;
Expression *eq_haa_a = NULL;
Expression *nat = NULL;
Context *ctx_with_axioms = NULL;
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
// with app_cong.
Context *extend_with_app_cong(Context *ctx) {

  Expression *A = init_var_expression("A");
  Expression *B = init_var_expression("B");
  Expression *f = init_var_expression("f");
  Expression *g = init_var_expression("g");
  Expression *x = init_var_expression("x");
  Expression *y = init_var_expression("y");

  Expression *type = init_type_expression();

  Context *base_ctx = extend_with_eq(context_create_empty());
  Context *A_ctx = context_insert(base_ctx, A, type);
  Context *B_ctx = context_insert(A_ctx, B, type);
  Context *f_ctx = context_insert(B_ctx, f, init_arrow_expression(B_ctx, A, B));
  Context *g_ctx = context_insert(f_ctx, g, init_arrow_expression(B_ctx, A, B));
  Context *x_ctx = context_insert(g_ctx, x, A);
  Context *full_ctx = context_insert(x_ctx, y, A);

  // app_cong_H1 = eq (A -> B) f g
  Expression *app_cong_H1 = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, init_arrow_expression(B_ctx, A, B)), f), g);

  // app_cong_H2 = eq A x y
  Expression *app_cong_H2 = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, A), x), y);

  // app_cong_concl = eq B (f x) (g y)
  Expression *f_x = init_app_expression(full_ctx, f, x);
  Expression *g_y = init_app_expression(full_ctx, g, y);
  Expression *app_cong_concl = init_app_expression(full_ctx, 
    init_app_expression(full_ctx, 
      init_app_expression(full_ctx, eq, B), f_x), g_y);

  Expression *app_cong_body = init_arrow_expression(full_ctx, app_cong_H1, 
    init_arrow_expression(full_ctx, app_cong_H2, app_cong_concl));

  // app_cong : forall (A B: Type) (f g : A -> B) (x y : A), f = g -> x = y -> f x = g y
  Expression *app_cong_ty = init_forall_expression(A_ctx, 
    init_forall_expression(B_ctx, 
      init_forall_expression(f_ctx, 
        init_forall_expression(g_ctx, 
          init_forall_expression(x_ctx, 
            init_forall_expression(full_ctx, app_cong_body))))));
  return context_insert(ctx, app_cong, app_cong_ty);
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

Context *extend_with_lambda_extensionality(Context *ctx) {
  Expression *type = init_type_expression();

  Expression *A = init_var_expression("A");
  Expression *B = init_var_expression("B");
  Expression *f = init_var_expression("f");
  Expression *g = init_var_expression("g");

  Context *base_ctx = extend_with_eq(extend_with_nat(context_create_empty()));
  Context *A_ctx = context_insert(base_ctx, A, type);
  Context *B_ctx = context_insert(A_ctx, B, type);

  Expression *fg_ty = init_arrow_expression(B_ctx, A, B);
  Context *f_ctx = context_insert(B_ctx, f, fg_ty);
  Context *g_ctx = context_insert(f_ctx, g, fg_ty);

  Expression *H = init_var_expression("_");
  Expression *x = init_var_expression("x");
  Context *H_ctx = context_insert(g_ctx, x, A);
  // H_ty is forall x: A, eq B (f x) (g x)
  Expression *H_ty = init_forall_expression(H_ctx, init_app_expression(H_ctx, 
    init_app_expression(H_ctx, 
      init_app_expression(H_ctx, eq, B), init_app_expression(H_ctx, f, x)), init_app_expression(H_ctx, g, x)));

  Context *final_ctx = context_insert(g_ctx, H, H_ty);
  Expression *lambda_extensionality_concl = init_app_expression(final_ctx, 
    init_app_expression(final_ctx, 
      init_app_expression(final_ctx, eq, init_arrow_expression(final_ctx, A, B)), f), g);

  Expression *lambda_extensionality_ty = init_forall_expression(A_ctx,
    init_forall_expression(B_ctx, 
      init_forall_expression(f_ctx, 
        init_forall_expression(g_ctx, 
          init_forall_expression(final_ctx, lambda_extensionality_concl)))));
    
  return context_insert(ctx, lambda_extensionality, lambda_extensionality_ty);
}

void init_globals() {
  if (!eq) eq = init_var_expression("eq");
  if (!eq_refl) eq_refl = init_var_expression("eq_refl");
  if (!app_cong) app_cong = init_var_expression("app_cong");
  if (!eq_trans) eq_trans = init_var_expression("eq_trans");
  if (!lambda_extensionality) lambda_extensionality = init_var_expression("lambda_extensionality");
  if (!f) f = init_var_expression("f");
  if (!g) g = init_var_expression("g");
  if (!h) h = init_var_expression("h");
  if (!a) a = init_var_expression("a");
  if (!b) b = init_var_expression("b");
  if (!c) c = init_var_expression("c");
  if (!eq_fa_a) eq_fa_a = init_var_expression("eq_fa_a");
  if (!eq_haa_a) eq_haa_a = init_var_expression("eq_haa_a");
  if (!nat) nat = init_var_expression("nat");

  if (!ctx_with_axioms) {
    ctx_with_axioms = context_create_empty();
    ctx_with_axioms = extend_with_nat(ctx_with_axioms);
    ctx_with_axioms = extend_with_eq(ctx_with_axioms);
    ctx_with_axioms = extend_with_app_cong(ctx_with_axioms);
    ctx_with_axioms = extend_with_eq_trans(ctx_with_axioms);
    ctx_with_axioms = extend_with_lambda_extensionality(ctx_with_axioms);  
  }

  if (!f_a_ctx) {
    f_a_ctx = ctx_with_axioms;

    Expression *f_ty = init_arrow_expression(f_a_ctx, nat, nat);
    Expression *a_ty = nat;

    f_a_ctx = context_insert(f_a_ctx, b, a_ty);
    f_a_ctx = context_insert(f_a_ctx, f, f_ty);
    f_a_ctx = context_insert(f_a_ctx, a, a_ty);

    Expression *fa_a_equality = init_app_expression(
        f_a_ctx, init_app_expression(f_a_ctx, 
          init_app_expression(f_a_ctx, eq, nat), init_app_expression(f_a_ctx, f, a)), a);

    // For all (a: nat), f a = a.
    Expression *eq_fa_a_ty = init_forall_expression(f_a_ctx, fa_a_equality);
    f_a_ctx = context_insert(f_a_ctx, eq_fa_a, eq_fa_a_ty);
  };

  if (!g_f_a_ctx) {
    Expression *g_ty = init_arrow_expression(f_a_ctx, nat, nat);
    g_f_a_ctx = context_insert(f_a_ctx, g, g_ty);
  };

  if (!h_g_f_a_ctx) {
    Expression *h_ty = init_arrow_expression(f_a_ctx, nat, init_arrow_expression(f_a_ctx, nat, nat));
    h_g_f_a_ctx = context_insert(g_f_a_ctx, h, h_ty);
    h_g_f_a_ctx = context_insert(h_g_f_a_ctx, c, nat);

    Expression *x = init_var_expression("x");
    Expression *y = init_var_expression("y");

    Context *x_ctx = context_insert(h_g_f_a_ctx, x, nat);
    h_g_f_a_ctx = context_insert(x_ctx, y, nat);

    Expression *hxy = init_app_expression(h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, x), y);

    Expression *hxy_c_equality = init_app_expression(h_g_f_a_ctx, 
      init_app_expression(h_g_f_a_ctx, 
        init_app_expression(h_g_f_a_ctx, eq, nat), hxy), c);

    Expression *hab_a_ty = init_forall_expression(x_ctx, init_forall_expression(h_g_f_a_ctx, hxy_c_equality));

    h_g_f_a_ctx = context_insert(h_g_f_a_ctx, eq_haa_a, hab_a_ty);
  }
}

bool _congruence(Expression *a, Expression *b, Map *mapping) {
  // Mapping is a map from variables in a to variables in b.
  if (a->type == PROP_EXPRESSION && b->type == TYPE_EXPRESSION) {
    return true;
  } else if (a->type != b->type) {
    return false;
  }

  switch (a->type) {
    case (TYPE_EXPRESSION): return true;
    case (PROP_EXPRESSION): return true;  
    case (APP_EXPRESSION): return _congruence(a->value.app.func, b->value.app.func, mapping) && _congruence(a->value.app.arg, b->value.app.arg, mapping);
    case (FORALL_EXPRESSION): {
      map_set(mapping, a->value.forall.bound_variable->variable, b->value.forall.bound_variable->variable);
      return _congruence(a->value.forall.body, b->value.forall.body, mapping);
    }
    case (LAMBDA_EXPRESSION): {
      map_set(mapping, a->value.lambda.bound_variable->variable, b->value.lambda.bound_variable->variable);
      return _congruence(a->value.lambda.body, b->value.lambda.body, mapping);
    }
    case (VAR_EXPRESSION): {
      return (a == b) || (map_get(mapping, a) == b);
    }
    case (HOLE_EXPRESSION): {
      return (a == b); // I suspect this should be the same as the VAR_EXPRESSION case
    }
  }
}

bool congruence(Expression *a, Expression *b) {
  Map *mapping = map_new();
  bool result = _congruence(a, b, mapping);
  free(mapping->items);
  free(mapping);
  return result;
}