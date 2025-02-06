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
  if (!nat) nat = init_var_expression("nat", init_type_expression());
  return context_insert(ctx, nat);
}

// Takes in a context in which nat is defined, and extends it with eq, eq_refl.
Context *extend_with_eq(Context *ctx) {
  // defining the eq type. eq : forall A : Type, A -> A -> Prop
  Expression *prop = init_prop_expression();
  Expression *type = init_type_expression();
  Expression *A = init_var_expression("A", type);
  Expression *eq_ty = init_forall_expression(A, init_arrow_expression(A, init_arrow_expression(A, prop)));
  if (!eq) eq = init_var_expression("eq", eq_ty);
  Context *ctx_with_eq = context_insert(ctx, eq);

  // defining the refl type. eq_refl : forall (B : Type) (x : B), eq B x x. 
  Expression *B = init_var_expression("B", type);
  Expression *x = init_var_expression("x", B);
  Expression *refl_ty = init_forall_expression(B, 
    init_forall_expression(x, 
      init_app_expression(init_app_expression(init_app_expression(eq, B), x), x)));
  if (!eq_refl) eq_refl = init_var_expression("eq_refl", refl_ty);
  Context *ctx_with_eq_and_refl = context_insert(ctx_with_eq, eq_refl);

  return ctx_with_eq_and_refl;
}

// Takes in a context which has nat and eq defined within it, and extends it
// with app_cong.
Context *extend_with_app_cong(Context *ctx) {
  Expression *type = init_type_expression();

  Expression *A = init_var_expression("A", type);
  Expression *B = init_var_expression("B", type);
  Expression *f = init_var_expression("f", init_arrow_expression(A, B));
  Expression *g = init_var_expression("g", init_arrow_expression(A, B));
  Expression *x = init_var_expression("x", A);
  Expression *y = init_var_expression("y", A);

  // app_cong_H1 = eq (A -> B) f g
  Expression *app_cong_H1 = init_app_expression(init_app_expression(init_app_expression(
    eq, init_arrow_expression(A, B)), f), g);

  // app_cong_H2 = eq A x y
  Expression *app_cong_H2 = init_app_expression(init_app_expression(init_app_expression(
    eq, A), x), y);

  // app_cong_concl = eq B (f x) (g y)
  Expression *f_x = init_app_expression(f, x);
  Expression *g_y = init_app_expression(g, y);
  Expression *app_cong_concl = init_app_expression(init_app_expression(init_app_expression(
    eq, B), f_x), g_y);

  Expression *app_cong_body = init_arrow_expression(app_cong_H1, 
    init_arrow_expression(app_cong_H2, app_cong_concl));

  // app_cong : forall (A B: Type) (f g : A -> B) (x y : A), f = g -> x = y -> f x = g y
  Expression *app_cong_ty = init_forall_expression(A, 
    init_forall_expression(B, 
      init_forall_expression(f, 
        init_forall_expression(g, 
          init_forall_expression(x, 
            init_forall_expression(y, app_cong_body))))));
  if (!app_cong) app_cong = init_var_expression("app_cong", app_cong_ty);
  return context_insert(ctx, app_cong);
}

// Takes in a context which has nat and eq defined within it, and extends it
// with eq_trans
Context *extend_with_eq_trans(Context *ctx) {
  // defining the eq_trans type
  Expression *x = init_var_expression("x", nat);
  Expression *y = init_var_expression("y", nat);
  Expression *z = init_var_expression("z", nat);

  Expression *eq_trans_H1 = init_app_expression(init_app_expression(init_app_expression(
    eq, nat), x), y);
  Expression *eq_trans_H2 = init_app_expression(init_app_expression(init_app_expression(
    eq, nat), y), z);
  Expression *eq_trans_concl = init_app_expression(init_app_expression(init_app_expression(
    eq, nat), x), z);

  Expression *eq_trans_body = init_arrow_expression(eq_trans_H1, 
    init_arrow_expression(eq_trans_H2, eq_trans_concl));

  Expression *eq_trans_ty = init_forall_expression(x, 
    init_forall_expression(y, 
      init_forall_expression(z, eq_trans_body)));

  if (!eq_trans) eq_trans = init_var_expression("eq_trans", eq_trans_ty);
  return context_insert(ctx, eq_trans);
}

Context *extend_with_lambda_extensionality(Context *ctx) {
  Expression *type = init_type_expression();

  Expression *A = init_var_expression("A", type);
  Expression *B = init_var_expression("B", type);

  Expression *fg_ty = init_arrow_expression(A, B);
  Expression *f = init_var_expression("f", fg_ty);
  Expression *g = init_var_expression("g", fg_ty);

  Expression *x = init_var_expression("x", A);
  // H_ty is forall x: A, eq B (f x) (g x)
  Expression *H_ty = init_forall_expression(x, 
    init_app_expression(init_app_expression(
      init_app_expression(eq, B), 
      init_app_expression(f, x)), 
    init_app_expression(g, x)));
  Expression *H = init_var_expression("_", H_ty);

  Expression *lambda_extensionality_concl = init_app_expression(
    init_app_expression(init_app_expression(eq, init_arrow_expression(A, B)), f), g);

  Expression *lambda_extensionality_ty = init_forall_expression(A,
    init_forall_expression(B, 
      init_forall_expression(f, 
        init_forall_expression(g, 
          init_forall_expression(H, lambda_extensionality_concl)))));
    
  if (!lambda_extensionality) lambda_extensionality = init_var_expression("lambda_extensionality", lambda_extensionality_ty);
  return context_insert(ctx, lambda_extensionality);
}

void init_globals() {
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

    Expression *f_ty = init_arrow_expression(nat, nat);
    Expression *a_ty = nat;

    if (!f) f = init_var_expression("f", f_ty);
    if (!a) a = init_var_expression("a", a_ty);
    if (!b) b = init_var_expression("b", a_ty);

    f_a_ctx = context_insert(f_a_ctx, b);
    f_a_ctx = context_insert(f_a_ctx, f);
    f_a_ctx = context_insert(f_a_ctx, a);

    Expression *fa_a_equality = init_app_expression(
        init_app_expression(init_app_expression(
          eq, nat), init_app_expression(f, a)), a);

    // forall (a: nat), f a = a.all
    Expression *eq_fa_a_ty = init_forall_expression(a, fa_a_equality);
    if (!eq_fa_a) eq_fa_a = init_var_expression("eq_fa_a", eq_fa_a_ty);
    f_a_ctx = context_insert(f_a_ctx, eq_fa_a);
  };

  if (!g_f_a_ctx) {
    Expression *g_ty = init_arrow_expression(nat, nat);
    if (!g) g = init_var_expression("g", g_ty);
    g_f_a_ctx = context_insert(f_a_ctx, g);
  };

  if (!h_g_f_a_ctx) {
    Expression *h_ty = init_arrow_expression(nat, init_arrow_expression(nat, nat));
    if (!h) h = init_var_expression("h", h_ty);
    if (!c) c = init_var_expression("c", nat);
    h_g_f_a_ctx = context_insert(g_f_a_ctx, h);
    h_g_f_a_ctx = context_insert(h_g_f_a_ctx, c);

    Expression *x = init_var_expression("x", nat);
    Expression *y = init_var_expression("y", nat);
    Context *x_ctx = context_insert(h_g_f_a_ctx, x);
    h_g_f_a_ctx = context_insert(x_ctx, y);

    Expression *hxy = init_app_expression(init_app_expression(h, x), y);

    Expression *hxy_c_equality = init_app_expression(init_app_expression(init_app_expression(
      eq, nat), hxy), c);

    Expression *hab_a_ty = init_forall_expression(x, init_forall_expression(y, hxy_c_equality));
    if (!eq_haa_a) eq_haa_a = init_var_expression("eq_haa_a", hab_a_ty);

    h_g_f_a_ctx = context_insert(h_g_f_a_ctx, eq_haa_a);
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
      map_set(mapping, a->value.forall.bound_variable, b->value.forall.bound_variable);
      return _congruence(a->value.forall.body, b->value.forall.body, mapping);
    }
    case (LAMBDA_EXPRESSION): {
      map_set(mapping, a->value.lambda.bound_variable, b->value.lambda.bound_variable);
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