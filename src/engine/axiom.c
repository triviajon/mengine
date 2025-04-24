#include "axiom.h"

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *eq_sym = NULL;
Expression *eq_subst = NULL;
Expression *app_cong = NULL;
Expression *eq_trans = NULL;
Expression *and = NULL;
Expression *and_conj = NULL;
Expression *not = NULL;
Expression *ex = NULL;
Expression *ex_intro = NULL;
Expression *lambda_extensionality = NULL;
Expression *f = NULL;
Expression *g = NULL;
Expression *h = NULL;
Expression *a = NULL;
Expression *b = NULL;
Expression *c = NULL;
Expression *eq_fa_a = NULL;
Expression *eq_hxx_x = NULL;
Expression *nat = NULL;
Expression *add = NULL;
Expression *add_r_O = NULL;
Expression *O = NULL;

Context *std_lib_ctx = NULL;

Context *init_and(Context *c) {
	and = init_var_expression("and", init_arrow_expression(init_prop_expression(), 
		init_arrow_expression(init_prop_expression(), init_prop_expression())));

	Expression *A = init_var_expression("A", init_prop_expression());
	Expression *B = init_var_expression("B", init_prop_expression());
	and_conj = init_var_expression("and_conj", init_forall_expression(A, 
		init_forall_expression(B, 
			init_arrow_expression(A, 
				init_arrow_expression(B, init_app_expression(init_app_expression(and, A), B))))));

  return context_insert_n(c, 2, and, and_conj);
}

Context *init_not(Context *c) {
  not = init_var_expression("not",  init_arrow_expression(init_prop_expression(), init_prop_expression()));
  return context_insert_n(c, 1, not);
}

Context *init_ex(Context *c) {
  Expression *A1 = init_var_expression("A", init_type_expression());
  Expression *P1 = init_var_expression("P", init_arrow_expression(A1, init_prop_expression()));
  ex = init_var_expression("ex", init_forall_expression(A1, 
    init_forall_expression(P1, init_prop_expression())));
  
  Expression *A2 = init_var_expression("A", init_type_expression());
  Expression *P2 = init_var_expression("P", init_arrow_expression(A2, init_prop_expression()));
  Expression *x2 = init_var_expression("x", A2);
  ex_intro = init_var_expression("ex_intro", init_forall_expression(A2, init_forall_expression(P2,
    init_forall_expression(x2, init_arrow_expression(init_app_expression(P2, x2),
      init_app_expression(init_app_expression(ex, A2), P2))))));

  return context_insert_n(c, 2, ex, ex_intro);
}

Context *init_nat(Context *c) {
  if (!nat) nat = init_var_expression("nat", init_type_expression());
  return context_insert_n(c, 1, nat);
}

Context *init_eq(Context *c) {
  // defining the eq type. eq : forall A : Type, A -> A -> Prop
  Expression *prop = init_prop_expression();
  Expression *type = init_type_expression();
  Expression *A = init_var_expression("A", type);
  Expression *eq_ty = init_forall_expression(A, init_arrow_expression(A, init_arrow_expression(A, prop)));
  if (!eq) eq = init_var_expression("eq", eq_ty);

  // defining the refl type. eq_refl : forall (B : Type) (x : B), eq B x x. 
  Expression *B = init_var_expression("B", type);
  Expression *x = init_var_expression("x", B);
  Expression *refl_ty = init_forall_expression(B, 
    init_forall_expression(x, 
      init_app_expression(init_app_expression(init_app_expression(eq, B), x), x)));
  if (!eq_refl) eq_refl = init_var_expression("eq_refl", refl_ty);

  // eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
  Expression *A_sym = init_var_expression("A", init_type_expression());
  Expression *x_sym = init_var_expression("x", A_sym);
  Expression *y_sym = init_var_expression("y", A_sym);
  Expression *H_sym = init_app_expression(init_app_expression(init_app_expression(eq, A_sym), x_sym), y_sym);
  Expression *C_sym = init_app_expression(init_app_expression(init_app_expression(eq, A_sym), y_sym), x_sym);
  Expression *sym_ty = init_forall_expression(A_sym, init_forall_expression(x_sym, init_forall_expression(y_sym, 
    init_arrow_expression(H_sym, C_sym))));
  if (!eq_sym) eq_sym = init_var_expression("eq_sym", sym_ty);

  // eq_subst: forall (P Q : Prop) (_: eq Prop P Q) (_: Q), P
  Expression *P3 = init_var_expression("P", init_prop_expression());
  Expression *Q3 = init_var_expression("Q", init_prop_expression());
  Expression *H3_1 = init_var_expression("_", init_app_expression(init_app_expression(init_app_expression(eq, init_prop_expression()), P3), Q3));
  Expression *H3_2 = init_var_expression("_", Q3);
  Expression *subst_ty = init_forall_expression(P3, init_forall_expression(Q3, 
    init_forall_expression(H3_1, init_forall_expression(H3_2, P3))));
  if (!eq_subst) eq_subst = init_var_expression("eq_subst", subst_ty);

  return context_insert_n(c, 4, eq, eq_refl, eq_sym, eq_subst);
}

Context *init_app_cong(Context *c) {
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

  return context_insert_n(c, 1, app_cong);
}

Context *init_eq_trans(Context *c) {
  // defining the eq_trans type
  Expression *A = init_var_expression("A", init_type_expression());
  Expression *x = init_var_expression("x", A);
  Expression *y = init_var_expression("y", A);
  Expression *z = init_var_expression("z", A);

  Expression *eq_trans_H1 = init_app_expression(init_app_expression(init_app_expression(
    eq, A), x), y);
  Expression *eq_trans_H2 = init_app_expression(init_app_expression(init_app_expression(
    eq, A), y), z);
  Expression *eq_trans_concl = init_app_expression(init_app_expression(init_app_expression(
    eq, A), x), z);

  Expression *eq_trans_body = init_arrow_expression(eq_trans_H1, 
    init_arrow_expression(eq_trans_H2, eq_trans_concl));

  Expression *eq_trans_ty = init_forall_expression(A, init_forall_expression(x, 
    init_forall_expression(y, 
      init_forall_expression(z, eq_trans_body))));

  if (!eq_trans) eq_trans = init_var_expression("eq_trans", eq_trans_ty);

  return context_insert_n(c, 1, eq_trans);
}

Context *init_lambda_extensionality(Context *c) {
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

  return context_insert_n(c, 1, lambda_extensionality);
}

Context *init_add(Context *c) {
    // Define addition. add : nat -> (nat -> nat).
  Expression *add_ty = init_arrow_expression(nat, init_arrow_expression(nat, nat));
  if (!add) add = init_var_expression("add", add_ty);
  if (!O) O = init_var_expression("O", nat);

  // Axiomize Lemma add_r_O : forall (n : nat), eq nat ((add n) O) n.
  Expression *n = init_var_expression("n", nat);
  Expression *add_r_O_ty = init_forall_expression(n,
    init_app_expression(
      init_app_expression(
        init_app_expression(eq, nat),
        init_app_expression(init_app_expression(add, n), O)),
    n)
  );
  if (!add_r_O) add_r_O = init_var_expression("add_r_O", add_r_O_ty); 

  return context_insert_n(c, 3, add, O, add_r_O);
}

Context *init_temporary(Context *ctx) {
  Expression *f_ty = init_arrow_expression(nat, nat);
  Expression *a_ty = nat;

  if (!f) f = init_var_expression("f", f_ty);
  if (!a) a = init_var_expression("a", a_ty);
  if (!b) b = init_var_expression("b", a_ty);

  Expression *fa_a_equality = init_app_expression(
      init_app_expression(init_app_expression(
        eq, nat), init_app_expression(f, a)), a);

  if (!eq_fa_a) eq_fa_a = init_var_expression("eq_fa_a", fa_a_equality);

  Expression *g_ty = init_arrow_expression(nat, nat);
  if (!g) g = init_var_expression("g", g_ty);

  Expression *h_ty = init_arrow_expression(nat, init_arrow_expression(nat, nat));
  if (!h) h = init_var_expression("h", h_ty);
  if (!c) c = init_var_expression("c", nat);

  Expression *x = init_var_expression("x", nat);

  Expression *hxx = init_app_expression(init_app_expression(h, x), x);

  Expression *hxx_x_equality = init_app_expression(init_app_expression(init_app_expression(
    eq, nat), hxx), x);

  Expression *hxx_x_ty = init_forall_expression(x, hxx_x_equality);
  if (!eq_hxx_x) eq_hxx_x = init_var_expression("eq_hxx_x", hxx_x_ty);

  return context_insert_n(ctx, 8, f, a, b, eq_fa_a, g, h, c, eq_hxx_x);
}

void init_globals() {
  Context *c = context_create_empty();

	c = init_and(c);
	c = init_not(c);
  c = init_ex(c);
  c = init_nat(c);
  c = init_eq(c);
  c = init_app_cong(c);
  c = init_eq_trans(c);
  c = init_lambda_extensionality(c);
  c = init_add(c);

  std_lib_ctx = c;

  init_temporary(c);
}