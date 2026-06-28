#include "core.h"

#include "src/kernel/kernel_api.h"

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

Expression *prop_and = NULL;
Expression *prop_conj = NULL;

Expression *prop_or = NULL;
Expression *or_introl = NULL;
Expression *or_intror = NULL;

Expression *prop_ex = NULL;
Expression *ex_intro = NULL;

static Context *init_Equivalence(Context *c) {
    // Reflexive : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Reflexive_Definition : forall (A : Type) (R : A -> A -> Prop), Reflexive
    // A R -> forall (x : A), R x x.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;

        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);

        Expression *reflexive_body = kernel_prop_create();
        Expression *reflexive_type =
            kernel_forall_create(A, kernel_forall_create(R, reflexive_body));
        Reflexive = kernel_var_create("Reflexive", reflexive_type, c);
    }

    c = Reflexive;

    {
        // Now build Reflexive_Definition
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = kernel_var_create("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *R_x_x = kernel_app_create(kernel_app_create(R, x, ctx_x), x, ctx_x);
        Expression *reflexive_def_body = kernel_forall_create(x, R_x_x);
        Expression *reflexive_def_type =
            kernel_arrow_create(kernel_app_create(kernel_app_create(Reflexive, A, ctx_R), R, ctx_R),
                                reflexive_def_body, ctx_R);
        reflexive_def_type = kernel_forall_create(R, reflexive_def_type);
        reflexive_def_type = kernel_forall_create(A, reflexive_def_type);

        Reflexive_Definition = kernel_var_create("Reflexive_Definition", reflexive_def_type, c);
    }

    c = Reflexive_Definition;

    // Symmetric : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Symmetric_Definition : forall (A : Type) (R : A -> A -> Prop), Symmetric
    // A R -> forall (x y : A), R x y -> R y x.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;

        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);

        Expression *symmetric_body = kernel_prop_create();
        Expression *symmetric_type =
            kernel_forall_create(A, kernel_forall_create(R, symmetric_body));
        Symmetric = kernel_var_create("Symmetric", symmetric_type, c);
    }

    c = Symmetric;

    {
        // Now build Symmetric_Definition
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = kernel_var_create("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *R_x_y = kernel_app_create(kernel_app_create(R, x, ctx_y), y, ctx_y);
        Expression *R_y_x = kernel_app_create(kernel_app_create(R, y, ctx_y), x, ctx_y);
        Expression *symmetric_def_body = kernel_arrow_create(R_x_y, R_y_x, ctx_y);
        symmetric_def_body = kernel_forall_create(y, symmetric_def_body);
        symmetric_def_body = kernel_forall_create(x, symmetric_def_body);
        Expression *symmetric_def_type =
            kernel_arrow_create(kernel_app_create(kernel_app_create(Symmetric, A, ctx_R), R, ctx_R),
                                symmetric_def_body, ctx_R);
        symmetric_def_type = kernel_forall_create(R, symmetric_def_type);
        symmetric_def_type = kernel_forall_create(A, symmetric_def_type);

        Symmetric_Definition = kernel_var_create("Symmetric_Definition", symmetric_def_type, c);
    }

    c = Symmetric_Definition;

    // Transitive : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Transitive_Definition : forall (A : Type) (R : A -> A -> Prop),
    // Transitive A R -> forall (x y z : A), R x y -> R y z -> R x z.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;

        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);

        Expression *transitive_body = kernel_prop_create();
        Expression *transitive_type =
            kernel_forall_create(A, kernel_forall_create(R, transitive_body));
        Transitive = kernel_var_create("Transitive", transitive_type, c);
    }

    c = Transitive;

    {
        // Now build Transitive_Definition
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *x = kernel_var_create("x", A, ctx_R);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *z = kernel_var_create("z", A, ctx_y);
        Context *ctx_z = z;
        Expression *R_x_y = kernel_app_create(kernel_app_create(R, x, ctx_z), y, ctx_z);
        Expression *R_y_z = kernel_app_create(kernel_app_create(R, y, ctx_z), z, ctx_z);
        Expression *R_x_z = kernel_app_create(kernel_app_create(R, x, ctx_z), z, ctx_z);
        Expression *transitive_def_body =
            kernel_arrow_create(R_x_y, kernel_arrow_create(R_y_z, R_x_z, ctx_z), ctx_z);
        transitive_def_body = kernel_forall_create(z, transitive_def_body);
        transitive_def_body = kernel_forall_create(y, transitive_def_body);
        transitive_def_body = kernel_forall_create(x, transitive_def_body);
        Expression *transitive_def_type = kernel_arrow_create(
            kernel_app_create(kernel_app_create(Transitive, A, ctx_R), R, ctx_R),
            transitive_def_body, ctx_R);
        transitive_def_type = kernel_forall_create(R, transitive_def_type);
        transitive_def_type = kernel_forall_create(A, transitive_def_type);

        Transitive_Definition = kernel_var_create("Transitive_Definition", transitive_def_type, c);
    }

    c = Transitive_Definition;

    // TODO: Skipping app congruence for now

    // Equivalence : forall (A : Type) (R : A -> A -> Prop), Prop.
    // Build_Equivalence : forall (A : Type) (R : A -> A -> Prop), Reflexive A R
    // -> Symmetric A R -> Transitive A R -> Equivalence A R.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);

        Expression *Equivalence_body = kernel_prop_create();
        Expression *Equivalence_type =
            kernel_forall_create(A, kernel_forall_create(R, Equivalence_body));
        Equivalence = kernel_var_create("Equivalence", Equivalence_type, c);
    }

    c = Equivalence;

    {
        // Now build Build_Equivalence
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;

        Expression *build_Equivalence_body =
            kernel_app_create(kernel_app_create(Equivalence, A, ctx_R), R, ctx_R);
        Expression *build_Equivalence_type = kernel_arrow_create(
            kernel_app_create(kernel_app_create(Reflexive, A, ctx_R), R, ctx_R),
            kernel_arrow_create(
                kernel_app_create(kernel_app_create(Symmetric, A, ctx_R), R, ctx_R),
                kernel_arrow_create(
                    kernel_app_create(kernel_app_create(Transitive, A, ctx_R), R, ctx_R),
                    build_Equivalence_body, ctx_R),
                ctx_R),
            ctx_R);
        build_Equivalence_type = kernel_forall_create(R, build_Equivalence_type);
        build_Equivalence_type = kernel_forall_create(A, build_Equivalence_type);
        Build_Equivalence = kernel_var_create("Build_Equivalence", build_Equivalence_type, c);
    }

    c = Build_Equivalence;

    // Equivalence_Reflexive : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Reflexive A R.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = kernel_var_create(
            "E", kernel_app_create(kernel_app_create(Equivalence, A, ctx_R), R, ctx_R), ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_reflexive_body =
            kernel_app_create(kernel_app_create(Reflexive, A, ctx_E), R, ctx_E);
        Expression *Equiv_reflexive_type = kernel_forall_create(E, Equiv_reflexive_body);
        Equiv_reflexive_type = kernel_forall_create(R, Equiv_reflexive_type);
        Equiv_reflexive_type = kernel_forall_create(A, Equiv_reflexive_type);

        Equivalence_Reflexive = kernel_var_create("Equivalence_Reflexive", Equiv_reflexive_type, c);
    }

    c = Equivalence_Reflexive;

    // Equivalence_Symmetric : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Symmetric A R.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = kernel_var_create(
            "E", kernel_app_create(kernel_app_create(Equivalence, A, ctx_R), R, ctx_R), ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_symmetric_body =
            kernel_app_create(kernel_app_create(Symmetric, A, ctx_E), R, ctx_E);
        Expression *Equiv_symmetric_type = kernel_forall_create(E, Equiv_symmetric_body);
        Equiv_symmetric_type = kernel_forall_create(R, Equiv_symmetric_type);
        Equiv_symmetric_type = kernel_forall_create(A, Equiv_symmetric_type);

        Equivalence_Symmetric = kernel_var_create("Equivalence_Symmetric", Equiv_symmetric_type, c);
    }

    c = Equivalence_Symmetric;

    // Equivalence_Transitive : forall (A : Type) (R : A -> A -> Prop) (E :
    // Equivalence A R), Transitive A R.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *R = kernel_var_create(
            "R", kernel_arrow_create(A, kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A),
            ctx_A);
        Context *ctx_R = R;
        Expression *E = kernel_var_create(
            "E", kernel_app_create(kernel_app_create(Equivalence, A, ctx_R), R, ctx_R), ctx_R);
        Context *ctx_E = E;

        Expression *Equiv_transitive_body =
            kernel_app_create(kernel_app_create(Transitive, A, ctx_E), R, ctx_E);
        Expression *Equiv_transitive_type = kernel_forall_create(E, Equiv_transitive_body);
        Equiv_transitive_type = kernel_forall_create(R, Equiv_transitive_type);
        Equiv_transitive_type = kernel_forall_create(A, Equiv_transitive_type);

        Equivalence_Transitive =
            kernel_var_create("Equivalence_Transitive", Equiv_transitive_type, c);
    }

    c = Equivalence_Transitive;
    // app_cong : forall (A B: Type) (f g : A -> B) (x y : A), R (A -> B) f g ->
    // R A x y -> f x = g y

    return c;
}

static Context *init_Eq(Context *c) {
    // eq : forall (A : Type) (x y : A), Prop.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *x = kernel_var_create("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);

        Expression *Eq_body = kernel_prop_create();
        Expression *Eq_type =
            kernel_forall_create(A, kernel_forall_create(x, kernel_forall_create(y, Eq_body)));
        eq = kernel_var_create("eq", Eq_type, c);
    }

    c = eq;

    // eq_refl : forall (A : Type) (x : A), eq A x x.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *x = kernel_var_create("x", A, ctx_A);
        Context *ctx_x = x;

        Expression *Eq_refl_body = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_x), x, ctx_x), x, ctx_x);
        Expression *Eq_refl_type = kernel_forall_create(A, kernel_forall_create(x, Eq_refl_body));
        eq_refl = kernel_var_create("eq_refl", Eq_refl_type, c);
    }

    c = eq_refl;

    // eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *x = kernel_var_create("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *Eq_A_x_y = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_y), x, ctx_y), y, ctx_y);
        Expression *Eq_A_y_x = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_y), y, ctx_y), x, ctx_y);

        Expression *Eq_sym_body = kernel_arrow_create(Eq_A_x_y, Eq_A_y_x, ctx_y);
        Expression *Eq_sym_type =
            kernel_forall_create(A, kernel_forall_create(x, kernel_forall_create(y, Eq_sym_body)));
        eq_sym = kernel_var_create("eq_sym", Eq_sym_type, c);
    }

    c = eq_sym;

    // eq_trans : forall (A : Type) (x y z : A), eq A x y -> eq A y z -> eq A x
    // z.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *x = kernel_var_create("x", A, ctx_A);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);
        Context *ctx_y = y;
        Expression *z = kernel_var_create("z", A, ctx_y);
        Context *ctx_z = z;

        Expression *Eq_A_x_y = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_z), x, ctx_z), y, ctx_z);
        Expression *Eq_A_y_z = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_z), y, ctx_z), z, ctx_z);
        Expression *Eq_A_x_z = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_z), x, ctx_z), z, ctx_z);

        Expression *Eq_trans_body =
            kernel_arrow_create(Eq_A_x_y, kernel_arrow_create(Eq_A_y_z, Eq_A_x_z, ctx_z), ctx_z);
        Expression *Eq_trans_type = kernel_forall_create(
            A, kernel_forall_create(
                   x, kernel_forall_create(y, kernel_forall_create(z, Eq_trans_body))));
        eq_trans = kernel_var_create("eq_trans", Eq_trans_type, c);
    }

    c = eq_trans;

    // eq_subst: forall (P Q : Prop) (_: eq Prop P Q) (_: Q), P
    {
        Expression *P = kernel_var_create("P", kernel_prop_create(), c);
        Context *ctx_P = P;
        Expression *Q = kernel_var_create("Q", kernel_prop_create(), ctx_P);
        Context *ctx_Q = Q;

        Expression *Eq_Prop_P_Q =
            kernel_app_create(kernel_app_create(eq, kernel_prop_create(), ctx_Q), P, ctx_Q);
        Eq_Prop_P_Q = kernel_app_create(Eq_Prop_P_Q, Q, ctx_Q);

        Expression *Eq_subst_body = P;
        Expression *Eq_subst_type = kernel_forall_create(
            P, kernel_forall_create(
                   Q, kernel_arrow_create(Eq_Prop_P_Q, kernel_arrow_create(Q, Eq_subst_body, ctx_Q),
                                          ctx_Q)));
        eq_subst = kernel_var_create("eq_subst", Eq_subst_type, c);
    }

    c = eq_subst;

    return c;
}

static Context *init_bad_app_congruence(Context *c) {
    // Bad_App_Congruence : forall (A B : Type) (f g : A -> B) (x y: A),
    //   eq (A -> B) f g -> eq A x y -> eq B (f x) (g y)
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *B = kernel_var_create("B", kernel_type_create(), ctx_A);
        Context *ctx_B = B;
        Expression *f = kernel_var_create("f", kernel_arrow_create(A, B, ctx_B), ctx_B);
        Context *ctx_f = f;
        Expression *g = kernel_var_create("g", kernel_arrow_create(A, B, ctx_f), ctx_f);
        Context *ctx_g = g;
        Expression *x = kernel_var_create("x", A, ctx_g);
        Context *ctx_x = x;
        Expression *y = kernel_var_create("y", A, ctx_x);
        Context *ctx_y = y;

        // eq (A -> B) f g
        Expression *Eq_A_to_B_f_g = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, kernel_arrow_create(A, B, ctx_y), ctx_y), f,
                              ctx_y),
            g, ctx_y);

        // eq A x y
        Expression *Eq_A_x_y = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, A, ctx_y), x, ctx_y), y, ctx_y);

        // f x and g y (applications)
        Expression *f_x = kernel_app_create(f, x, ctx_y);
        Expression *g_y = kernel_app_create(g, y, ctx_y);

        // eq B (f x) (g y)
        Expression *Eq_B_f_x_g_y = kernel_app_create(
            kernel_app_create(kernel_app_create(eq, B, ctx_y), f_x, ctx_y), g_y, ctx_y);

        // eq (A -> B) f g -> eq A x y -> eq B (f x) (g y)
        Expression *bad_app_congruence_body = kernel_arrow_create(
            Eq_A_to_B_f_g, kernel_arrow_create(Eq_A_x_y, Eq_B_f_x_g_y, ctx_y), ctx_y);

        // forall (A B : Type) (f g : A -> B) (x y: A), ...
        Expression *bad_app_congruence_type = kernel_forall_create(
            A, kernel_forall_create(
                   B, kernel_forall_create(
                          f, kernel_forall_create(
                                 g, kernel_forall_create(
                                        x, kernel_forall_create(y, bad_app_congruence_body))))));

        Bad_App_Congruence = kernel_var_create("Bad_App_Congruence", bad_app_congruence_type, c);
    }

    c = Bad_App_Congruence;

    return c;
}

Expression *_get_lhs_eq(Expression *eq_expression) {
    // eq_expression is of the form `eq A x y`; return NULL when it is not a fully
    // applied equality (e.g. a partially-applied or non-eq type), so callers fail
    // cleanly instead of dereferencing a missing application argument.
    if (!eq_expression || !kernel_expr_is_app(eq_expression)) {
        return NULL;
    }
    Expression *eq_app = kernel_app_func(eq_expression);
    if (!eq_app || !kernel_expr_is_app(eq_app)) {
        return NULL;
    }
    return kernel_app_arg(eq_app);
}

Expression *_get_rhs_eq(Expression *eq_expression) {
    // eq_expression is of the form eq A x y
    return kernel_app_arg(eq_expression);
}

static Context *init_and_or_ex(Context *c) {
    // and : Prop -> Prop -> Prop.
    {
        Expression *and_type = kernel_arrow_create(
            kernel_prop_create(),
            kernel_arrow_create(kernel_prop_create(), kernel_prop_create(), c), c);
        prop_and = kernel_var_create("and", and_type, c);
    }
    c = prop_and;

    // conj : forall (A B : Prop), A -> B -> and A B.
    {
        Expression *A = kernel_var_create("A", kernel_prop_create(), c);
        Context *ctx_A = A;
        Expression *B = kernel_var_create("B", kernel_prop_create(), ctx_A);
        Context *ctx_B = B;

        Expression *and_A_B = kernel_app_create(kernel_app_create(prop_and, A, ctx_B), B, ctx_B);
        Expression *conj_body =
            kernel_arrow_create(A, kernel_arrow_create(B, and_A_B, ctx_B), ctx_B);
        Expression *conj_type = kernel_forall_create(A, kernel_forall_create(B, conj_body));
        prop_conj = kernel_var_create("conj", conj_type, c);
    }
    c = prop_conj;

    // or : Prop -> Prop -> Prop.
    {
        Expression *or_type = kernel_arrow_create(
            kernel_prop_create(),
            kernel_arrow_create(kernel_prop_create(), kernel_prop_create(), c), c);
        prop_or = kernel_var_create("or", or_type, c);
    }
    c = prop_or;

    // or_introl : forall (A B : Prop), A -> or A B.
    {
        Expression *A = kernel_var_create("A", kernel_prop_create(), c);
        Context *ctx_A = A;
        Expression *B = kernel_var_create("B", kernel_prop_create(), ctx_A);
        Context *ctx_B = B;

        Expression *or_A_B = kernel_app_create(kernel_app_create(prop_or, A, ctx_B), B, ctx_B);
        Expression *or_introl_body = kernel_arrow_create(A, or_A_B, ctx_B);
        Expression *or_introl_type =
            kernel_forall_create(A, kernel_forall_create(B, or_introl_body));
        or_introl = kernel_var_create("or_introl", or_introl_type, c);
    }
    c = or_introl;

    // or_intror : forall (A B : Prop), B -> or A B.
    {
        Expression *A = kernel_var_create("A", kernel_prop_create(), c);
        Context *ctx_A = A;
        Expression *B = kernel_var_create("B", kernel_prop_create(), ctx_A);
        Context *ctx_B = B;

        Expression *or_A_B = kernel_app_create(kernel_app_create(prop_or, A, ctx_B), B, ctx_B);
        Expression *or_intror_body = kernel_arrow_create(B, or_A_B, ctx_B);
        Expression *or_intror_type =
            kernel_forall_create(A, kernel_forall_create(B, or_intror_body));
        or_intror = kernel_var_create("or_intror", or_intror_type, c);
    }
    c = or_intror;

    // ex : forall (A : Type) (P : A -> Prop), Prop.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *P =
            kernel_var_create("P", kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A);

        Expression *ex_type =
            kernel_forall_create(A, kernel_forall_create(P, kernel_prop_create()));
        prop_ex = kernel_var_create("ex", ex_type, c);
    }
    c = prop_ex;

    // ex_intro : forall (A : Type) (P : A -> Prop) (x : A), P x -> ex A P.
    {
        Expression *A = kernel_var_create("A", kernel_type_create(), c);
        Context *ctx_A = A;
        Expression *P =
            kernel_var_create("P", kernel_arrow_create(A, kernel_prop_create(), ctx_A), ctx_A);
        Context *ctx_P = P;
        Expression *x = kernel_var_create("x", A, ctx_P);
        Context *ctx_x = x;

        Expression *P_x = kernel_app_create(P, x, ctx_x);
        Expression *ex_A_P = kernel_app_create(kernel_app_create(prop_ex, A, ctx_x), P, ctx_x);
        Expression *ex_intro_body = kernel_arrow_create(P_x, ex_A_P, ctx_x);
        Expression *ex_intro_type = kernel_forall_create(
            A, kernel_forall_create(P, kernel_forall_create(x, ex_intro_body)));
        ex_intro = kernel_var_create("ex_intro", ex_intro_type, c);
    }
    c = ex_intro;

    return c;
}

void init_core(Context **ctx) {
    if (*ctx == NULL) {
        *ctx = kernel_context_empty();
    }
    *ctx = init_Equivalence(*ctx);
    *ctx = init_Eq(*ctx);
    *ctx = init_bad_app_congruence(*ctx);
    *ctx = init_and_or_ex(*ctx);
}