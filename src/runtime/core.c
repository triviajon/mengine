#include "core.h"

#include "src/kernel/context.h"
#include "src/kernel/expression.h"

Expression *Reflexive = NULL;
Expression *Reflexive_Definition = NULL;

Expression *Symmetric = NULL;
Expression *Symmetric_Definition = NULL;

Expression *Transitive = NULL;
Expression *Transitive_Definition = NULL;

Expression *Equivalence = NULL;
Expression *Build_Equivalence = NULL;

Expression *Equivalence_Reflexive = NULL;
Expression *Equivalence_Symmetric = NULL;
Expression *Equivalence_Transitive = NULL;

Expression *Bad_App_Congruence = NULL;

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *eq_sym = NULL;
Expression *eq_trans = NULL;
Expression *eq_subst = NULL;

static Context *init_Equivalence(Context *c) {
    // Reflexive : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Reflexive_Definition : forall (A : Type) (R : A -> A -> Prop), Reflexive
    // A R -> forall (x : A), R x x.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;

        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);

        Expression *reflexive_body = init_prop_expression();
        Expression *reflexive_type =
            init_forall_expression_wc(A, init_forall_expression_wc(R, reflexive_body));
        Reflexive = init_var_expression_wc("Reflexive", reflexive_type, c);
    }

    c = Reflexive;

    {
        // Now build Reflexive_Definition
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *R_x_x = init_app_expression_wc(init_app_expression_wc(R, x, ctx_x), x, ctx_x);
        Expression *reflexive_def_body = init_forall_expression_wc(x, R_x_x);
        Expression *reflexive_def_type = init_arrow_expression_wc(
            init_app_expression_wc(init_app_expression_wc(Reflexive, A, ctx_R), R, ctx_R),
            reflexive_def_body, ctx_R);
        reflexive_def_type = init_forall_expression_wc(R, reflexive_def_type);
        reflexive_def_type = init_forall_expression_wc(A, reflexive_def_type);

        Reflexive_Definition =
            init_var_expression_wc("Reflexive_Definition", reflexive_def_type, c);
    }

    c = Reflexive_Definition;

    // Symmetric : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Symmetric_Definition : forall (A : Type) (R : A -> A -> Prop), Symmetric
    // A R -> forall (x y : A), R x y -> R y x.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;

        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);

        Expression *symmetric_body = init_prop_expression();
        Expression *symmetric_type =
            init_forall_expression_wc(A, init_forall_expression_wc(R, symmetric_body));
        Symmetric = init_var_expression_wc("Symmetric", symmetric_type, c);
    }

    c = Symmetric;

    {
        // Now build Symmetric_Definition
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *R_x_y = init_app_expression_wc(init_app_expression_wc(R, x, ctx_y), y, ctx_y);
        Expression *R_y_x = init_app_expression_wc(init_app_expression_wc(R, y, ctx_y), x, ctx_y);
        Expression *symmetric_def_body = init_arrow_expression_wc(R_x_y, R_y_x, ctx_y);
        symmetric_def_body = init_forall_expression_wc(y, symmetric_def_body);
        symmetric_def_body = init_forall_expression_wc(x, symmetric_def_body);
        Expression *symmetric_def_type = init_arrow_expression_wc(
            init_app_expression_wc(init_app_expression_wc(Symmetric, A, ctx_R), R, ctx_R),
            symmetric_def_body, ctx_R);
        symmetric_def_type = init_forall_expression_wc(R, symmetric_def_type);
        symmetric_def_type = init_forall_expression_wc(A, symmetric_def_type);

        Symmetric_Definition =
            init_var_expression_wc("Symmetric_Definition", symmetric_def_type, c);
    }

    c = Symmetric_Definition;

    // Transitive : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Transitive_Definition : forall (A : Type) (R : A -> A -> Prop),
    // Transitive A R -> forall (x y z : A), R x y -> R y z -> R x z.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;

        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);

        Expression *transitive_body = init_prop_expression();
        Expression *transitive_type =
            init_forall_expression_wc(A, init_forall_expression_wc(R, transitive_body));
        Transitive = init_var_expression_wc("Transitive", transitive_type, c);
    }

    c = Transitive;

    {
        // Now build Transitive_Definition
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *z = init_var_expression_wc("z", A, ctx_y);
        Context *ctx_z = z;
        Expression *R_x_y = init_app_expression_wc(init_app_expression_wc(R, x, ctx_z), y, ctx_z);
        Expression *R_y_z = init_app_expression_wc(init_app_expression_wc(R, y, ctx_z), z, ctx_z);
        Expression *R_x_z = init_app_expression_wc(init_app_expression_wc(R, x, ctx_z), z, ctx_z);
        Expression *transitive_def_body =
            init_arrow_expression_wc(R_x_y, init_arrow_expression_wc(R_y_z, R_x_z, ctx_z), ctx_z);
        transitive_def_body = init_forall_expression_wc(z, transitive_def_body);
        transitive_def_body = init_forall_expression_wc(y, transitive_def_body);
        transitive_def_body = init_forall_expression_wc(x, transitive_def_body);
        Expression *transitive_def_type = init_arrow_expression_wc(
            init_app_expression_wc(init_app_expression_wc(Transitive, A, ctx_R), R, ctx_R),
            transitive_def_body, ctx_R);
        transitive_def_type = init_forall_expression_wc(R, transitive_def_type);
        transitive_def_type = init_forall_expression_wc(A, transitive_def_type);

        Transitive_Definition =
            init_var_expression_wc("Transitive_Definition", transitive_def_type, c);
    }

    c = Transitive_Definition;

    // TODO: Skipping app congruence for now

    // Equivalence : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Build_Equivalence : forall (A : Type) (R : A -> A -> Prop), Reflexive A R
    // -> Symmetric A R -> Transitive A R -> Equivalence A R.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);

        Expression *Equivalence_body = init_prop_expression();
        Expression *Equivalence_type =
            init_forall_expression_wc(A, init_forall_expression_wc(R, Equivalence_body));
        Equivalence = init_var_expression_wc("Equivalence", Equivalence_type, c);
    }

    c = Equivalence;

    {
        // Now build Build_Equivalence
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *build_Equivalence_body =
            init_app_expression_wc(init_app_expression_wc(Equivalence, A, ctx_R), R, ctx_R);
        Expression *build_Equivalence_type = init_arrow_expression_wc(
            init_app_expression_wc(init_app_expression_wc(Reflexive, A, ctx_R), R, ctx_R),
            init_arrow_expression_wc(
                init_app_expression_wc(init_app_expression_wc(Symmetric, A, ctx_R), R, ctx_R),
                init_arrow_expression_wc(
                    init_app_expression_wc(init_app_expression_wc(Transitive, A, ctx_R), R, ctx_R),
                    build_Equivalence_body, ctx_R),
                ctx_R),
            ctx_R);
        build_Equivalence_type = init_forall_expression_wc(R, build_Equivalence_type);
        build_Equivalence_type = init_forall_expression_wc(A, build_Equivalence_type);
        Build_Equivalence = init_var_expression_wc("Build_Equivalence", build_Equivalence_type, c);
    }

    c = Build_Equivalence;

    // Equivalence_Reflexive : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Reflexive A R.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = init_var_expression_wc(
            "E", init_app_expression_wc(init_app_expression_wc(Equivalence, A, ctx_R), R, ctx_R),
            ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_reflexive_body =
            init_app_expression_wc(init_app_expression_wc(Reflexive, A, ctx_E), R, ctx_E);
        Expression *Equiv_reflexive_type = init_forall_expression_wc(E, Equiv_reflexive_body);
        Equiv_reflexive_type = init_forall_expression_wc(R, Equiv_reflexive_type);
        Equiv_reflexive_type = init_forall_expression_wc(A, Equiv_reflexive_type);

        Equivalence_Reflexive =
            init_var_expression_wc("Equivalence_Reflexive", Equiv_reflexive_type, c);
    }

    c = Equivalence_Reflexive;

    // Equivalence_Symmetric : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Symmetric A R.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = init_var_expression_wc(
            "E", init_app_expression_wc(init_app_expression_wc(Equivalence, A, ctx_R), R, ctx_R),
            ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_symmetric_body =
            init_app_expression_wc(init_app_expression_wc(Symmetric, A, ctx_E), R, ctx_E);
        Expression *Equiv_symmetric_type = init_forall_expression_wc(E, Equiv_symmetric_body);
        Equiv_symmetric_type = init_forall_expression_wc(R, Equiv_symmetric_type);
        Equiv_symmetric_type = init_forall_expression_wc(A, Equiv_symmetric_type);

        Equivalence_Symmetric =
            init_var_expression_wc("Equivalence_Symmetric", Equiv_symmetric_type, c);
    }

    c = Equivalence_Symmetric;

    // Equivalence_Transitive : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Transitive A R.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *R = init_var_expression_wc(
            "R",
            init_arrow_expression_wc(A, init_arrow_expression_wc(A, init_prop_expression(), ctx_A),
                                     ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = init_var_expression_wc(
            "E", init_app_expression_wc(init_app_expression_wc(Equivalence, A, ctx_R), R, ctx_R),
            ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_transitive_body =
            init_app_expression_wc(init_app_expression_wc(Transitive, A, ctx_E), R, ctx_E);
        Expression *Equiv_transitive_type = init_forall_expression_wc(E, Equiv_transitive_body);
        Equiv_transitive_type = init_forall_expression_wc(R, Equiv_transitive_type);
        Equiv_transitive_type = init_forall_expression_wc(A, Equiv_transitive_type);

        Equivalence_Transitive =
            init_var_expression_wc("Equivalence_Transitive", Equiv_transitive_type, c);
    }

    c = Equivalence_Transitive;
    // app_cong : forall (A B: Type) (f g : A -> B) (x y : A), R (A -> B) f g ->
    // R A x y -> f x = g y

    return c;
}

static Context *init_Eq(Context *c) {
    // eq : forall (A : Type) (x y : A), Prop.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *x = init_var_expression_wc("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);

        Expression *Eq_body = init_prop_expression();
        Expression *Eq_type = init_forall_expression_wc(
            A, init_forall_expression_wc(x, init_forall_expression_wc(y, Eq_body)));
        eq = init_var_expression_wc("eq", Eq_type, c);
    }

    c = eq;

    // eq_refl : forall (A : Type) (x : A), eq A x x.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *x = init_var_expression_wc("x", A, ctx_A);
        Context *ctx_x = x;

        Expression *Eq_refl_body = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_x), x, ctx_x), x, ctx_x);
        Expression *Eq_refl_type =
            init_forall_expression_wc(A, init_forall_expression_wc(x, Eq_refl_body));
        eq_refl = init_var_expression_wc("eq_refl", Eq_refl_type, c);
    }

    c = eq_refl;

    // eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *x = init_var_expression_wc("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *Eq_A_x_y = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_y), x, ctx_y), y, ctx_y);
        Expression *Eq_A_y_x = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_y), y, ctx_y), x, ctx_y);

        Expression *Eq_sym_body = init_arrow_expression_wc(Eq_A_x_y, Eq_A_y_x, ctx_y);
        Expression *Eq_sym_type = init_forall_expression_wc(
            A, init_forall_expression_wc(x, init_forall_expression_wc(y, Eq_sym_body)));
        eq_sym = init_var_expression_wc("eq_sym", Eq_sym_type, c);
    }

    c = eq_sym;

    // eq_trans : forall (A : Type) (x y z : A), eq A x y -> eq A y z -> eq A x
    // z.
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *x = init_var_expression_wc("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *z = init_var_expression_wc("z", A, ctx_y);
        Context *ctx_z = z;

        Expression *Eq_A_x_y = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_z), x, ctx_z), y, ctx_z);
        Expression *Eq_A_y_z = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_z), y, ctx_z), z, ctx_z);
        Expression *Eq_A_x_z = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_z), x, ctx_z), z, ctx_z);

        Expression *Eq_trans_body = init_arrow_expression_wc(
            Eq_A_x_y, init_arrow_expression_wc(Eq_A_y_z, Eq_A_x_z, ctx_z), ctx_z);
        Expression *Eq_trans_type = init_forall_expression_wc(
            A, init_forall_expression_wc(
                   x, init_forall_expression_wc(y, init_forall_expression_wc(z, Eq_trans_body))));
        eq_trans = init_var_expression_wc("eq_trans", Eq_trans_type, c);
    }

    c = eq_trans;

    // eq_subst: forall (P Q : Prop) (_: eq Prop P Q) (_: Q), P
    {
        Expression *P = init_var_expression_wc("P", init_prop_expression(), c);
        Context *ctx_P = P;
        Expression *Q = init_var_expression_wc("Q", init_prop_expression(), ctx_P);
        Context *ctx_Q = Q;

        Expression *Eq_Prop_P_Q = init_app_expression_wc(
            init_app_expression_wc(eq, init_prop_expression(), ctx_Q), P, ctx_Q);
        Eq_Prop_P_Q = init_app_expression_wc(Eq_Prop_P_Q, Q, ctx_Q);

        Expression *Eq_subst_body = P;
        Expression *Eq_subst_type = init_forall_expression_wc(
            P, init_forall_expression_wc(
                   Q, init_arrow_expression_wc(
                          Eq_Prop_P_Q, init_arrow_expression_wc(Q, Eq_subst_body, ctx_Q), ctx_Q)));
        eq_subst = init_var_expression_wc("eq_subst", Eq_subst_type, c);
    }

    c = eq_subst;

    return c;
}

static Context *init_bad_app_congruence(Context *c) {
    // Bad_App_Congruence : forall (A B : Type) (f g : A -> B) (x y: A),
    //   eq (A -> B) f g -> eq A x y -> eq B (f x) (g y)
    {
        Expression *A = init_var_expression_wc("A", init_type_expression(), c);
        Context *ctx_A = A;
        Expression *B = init_var_expression_wc("B", init_type_expression(), ctx_A);
        Context *ctx_B = B;
        Expression *f = init_var_expression_wc("f", init_arrow_expression_wc(A, B, ctx_B), ctx_B);
        Context *ctx_f = f;
        Expression *g = init_var_expression_wc("g", init_arrow_expression_wc(A, B, ctx_f), ctx_f);
        Context *ctx_g = g;
        Expression *x = init_var_expression_wc("x", A, ctx_g);
        Context *ctx_x = x;
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = y;

        // eq (A -> B) f g
        Expression *Eq_A_to_B_f_g = init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(eq, init_arrow_expression_wc(A, B, ctx_y), ctx_y), f, ctx_y),
            g, ctx_y);

        // eq A x y
        Expression *Eq_A_x_y = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, A, ctx_y), x, ctx_y), y, ctx_y);

        // f x and g y (applications)
        Expression *f_x = init_app_expression_wc(f, x, ctx_y);
        Expression *g_y = init_app_expression_wc(g, y, ctx_y);

        // eq B (f x) (g y)
        Expression *Eq_B_f_x_g_y = init_app_expression_wc(
            init_app_expression_wc(init_app_expression_wc(eq, B, ctx_y), f_x, ctx_y), g_y, ctx_y);

        // eq (A -> B) f g -> eq A x y -> eq B (f x) (g y)
        Expression *bad_app_congruence_body = init_arrow_expression_wc(
            Eq_A_to_B_f_g, init_arrow_expression_wc(Eq_A_x_y, Eq_B_f_x_g_y, ctx_y), ctx_y);

        // forall (A B : Type) (f g : A -> B) (x y: A), ...
        Expression *bad_app_congruence_type = init_forall_expression_wc(
            A,
            init_forall_expression_wc(
                B, init_forall_expression_wc(
                       f, init_forall_expression_wc(
                              g, init_forall_expression_wc(
                                     x, init_forall_expression_wc(y, bad_app_congruence_body))))));

        Bad_App_Congruence =
            init_var_expression_wc("Bad_App_Congruence", bad_app_congruence_type, c);
    }

    c = Bad_App_Congruence;

    return c;
}

Expression *_get_lhs_eq(Expression *eq_expression) {
    // eq_expression is of the form eq A x y
    return get_app_arg(get_app_func(eq_expression));
}

Expression *_get_rhs_eq(Expression *eq_expression) {
    // eq_expression is of the form eq A x y
    return get_app_arg(eq_expression);
}

void init_core(Context **ctx) {
    if (*ctx == NULL) {
        *ctx = context_create_empty();
    }
    *ctx = init_Equivalence(*ctx);
    *ctx = init_Eq(*ctx);
    *ctx = init_bad_app_congruence(*ctx);
}