#include "src/engine/axiom.h"

Expression *eq = NULL;
Expression *eq_refl = NULL;
Expression *eq_sym = NULL;
Expression *eq_subst = NULL;
Expression *app_cong = NULL;
Expression *eq_trans = NULL;
Expression *and = NULL;
Expression *and_conj = NULL;
Expression * or = NULL;
Expression *or_introl = NULL;
Expression *or_intror = NULL;
Expression *iff1 = NULL;
Expression *iff1_refl = NULL;
Expression *iff1_sym = NULL;
Expression *iff1_trans = NULL;
Expression *iff1_subst = NULL;
Expression *iff1_cong = NULL;
Expression *not= NULL;
Expression *ex = NULL;
Expression *ex_intro = NULL;
Expression *True = NULL;
Expression *I = NULL;
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
Expression *option = NULL;
Expression *option_some = NULL;
Expression *option_none = NULL;
Expression *partial_map = NULL;
Expression *partial_map_empty = NULL;
Expression *partial_map_get = NULL;
Expression *partial_map_put = NULL;
Expression *partial_map_remove = NULL;
Expression *partial_map_get_put_same = NULL;
Expression *partial_map_get_put_diff = NULL;
Expression *partial_map_put_put_same = NULL;

Context *std_lib_ctx = NULL;

Context *init_and(Context *c) {
    and= init_var_expression(
        "and",
        init_arrow_expression(init_prop_expression(),
                              init_arrow_expression(init_prop_expression(),
                                                    init_prop_expression())));

    Expression *A = init_var_expression("A", init_prop_expression());
    Expression *B = init_var_expression("B", init_prop_expression());
    and_conj = init_var_expression(
        "and_conj",
        init_forall_expression(
            A, init_forall_expression(
                   B, init_arrow_expression(
                          A, init_arrow_expression(
                                 B, init_app_expression(
                                        init_app_expression(and, A), B))))));

    return context_insert_n(c, 2, and, and_conj);
}

Context *init_or(Context *c) {
    or = init_var_expression(
           "or", init_arrow_expression(
                     init_prop_expression(),
                     init_arrow_expression(init_prop_expression(),
                                           init_prop_expression())));

    Expression *A = init_var_expression("A", init_prop_expression());
    Expression *B = init_var_expression("B", init_prop_expression());
    or_introl = init_var_expression(
        "or_introl",
        init_forall_expression(
            A, init_forall_expression(
                   B, init_arrow_expression(
                          A, init_app_expression(init_app_expression(or, A),
                                                 B)))));

    or_intror = init_var_expression(
        "or_intror",
        init_forall_expression(
            A, init_forall_expression(
                   B, init_arrow_expression(
                          B, init_app_expression(init_app_expression(or, A),
                                                 B)))));

    return context_insert_n(c, 3, or, or_introl, or_intror);
}

Context *init_not(Context *c) {
    not= init_var_expression(
        "not",
        init_arrow_expression(init_prop_expression(), init_prop_expression()));
    return context_insert_n(c, 1, not);
}

Context *init_ex(Context *c) {
    Expression *A1 = init_var_expression("A", init_type_expression());
    Expression *P1 = init_var_expression(
        "P", init_arrow_expression(A1, init_prop_expression()));
    ex = init_var_expression(
        "ex", init_forall_expression(
                  A1, init_forall_expression(P1, init_prop_expression())));

    Expression *A2 = init_var_expression("A", init_type_expression());
    Expression *P2 = init_var_expression(
        "P", init_arrow_expression(A2, init_prop_expression()));
    Expression *x2 = init_var_expression("x", A2);
    ex_intro = init_var_expression(
        "ex_intro",
        init_forall_expression(
            A2, init_forall_expression(
                    P2, init_forall_expression(
                            x2, init_arrow_expression(
                                    init_app_expression(P2, x2),
                                    init_app_expression(
                                        init_app_expression(ex, A2), P2))))));

    return context_insert_n(c, 2, ex, ex_intro);
}

Context *init_iff1(Context *c) {
    // iff1 : forall T : Type, (T -> Prop) -> (T -> Prop) -> Prop
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *T_Prop = init_arrow_expression(T, init_prop_expression());
        iff1 = init_var_expression(
            "iff1", init_forall_expression(
                        T, init_arrow_expression(
                               T_Prop, init_arrow_expression(
                                           T_Prop, init_prop_expression()))));
    }

    // iff1_refl : forall T : Type, (P : T -> Prop), iff1 P P
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *P = init_var_expression(
            "P", init_arrow_expression(T, init_prop_expression()));
        iff1_refl = init_var_expression(
            "iff1_refl",
            init_forall_expression(
                T,
                init_forall_expression(
                    P, init_app_expression(
                           init_app_expression(init_app_expression(iff1, T), P),
                           P))));
    }

    // iff1_sym : forall T : Type, (P Q : T -> Prop), iff1 P Q -> iff1 Q P
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *P = init_var_expression(
            "P", init_arrow_expression(T, init_prop_expression()));
        Expression *Q = init_var_expression(
            "Q", init_arrow_expression(T, init_prop_expression()));
        Expression *iff1_P_Q = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), Q);
        Expression *iff1_Q_P = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), Q), P);

        iff1_sym = init_var_expression(
            "iff1_sym",
            init_forall_expression(
                T, init_forall_expression(
                       P, init_forall_expression(
                              Q, init_arrow_expression(iff1_P_Q, iff1_Q_P)))));
    }

    // iff1_trans : forall T : Type, (P Q R : T -> Prop), iff1 P Q -> iff1 Q R
    // -> iff1 P R
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *P = init_var_expression(
            "P", init_arrow_expression(T, init_prop_expression()));
        Expression *Q = init_var_expression(
            "Q", init_arrow_expression(T, init_prop_expression()));
        Expression *R = init_var_expression(
            "R", init_arrow_expression(T, init_prop_expression()));

        Expression *iff1_P_Q = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), Q);
        Expression *iff1_Q_R = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), Q), R);
        Expression *iff1_P_R = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), R);

        iff1_trans = init_var_expression(
            "iff1_trans",
            init_forall_expression(
                T, init_forall_expression(
                       P, init_forall_expression(
                              Q, init_forall_expression(
                                     R, init_arrow_expression(
                                            iff1_P_Q,
                                            init_arrow_expression(
                                                iff1_Q_R, iff1_P_R)))))));
    }

    // iff1_subst : forall T : Type, (P Q R : T -> Prop),, iff1 Q R -> iff1 P Q
    // -> iff1 P R
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *P = init_var_expression(
            "P", init_arrow_expression(T, init_prop_expression()));
        Expression *Q = init_var_expression(
            "Q", init_arrow_expression(T, init_prop_expression()));
        Expression *R = init_var_expression(
            "R", init_arrow_expression(T, init_prop_expression()));

        Expression *iff1_Q_R = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), Q), R);
        Expression *iff1_P_Q = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), Q);
        Expression *iff1_P_R = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), R);

        iff1_subst = init_var_expression(
            "iff1_subst",
            init_forall_expression(
                T, init_forall_expression(
                       P, init_forall_expression(
                              Q, init_forall_expression(
                                     R, init_arrow_expression(
                                            iff1_Q_R,
                                            init_arrow_expression(
                                                iff1_P_Q, iff1_P_R)))))));
    }

    // iff1_cong : forall (T: Type), (P Q R S : T -> Prop), iff1 P Q -> iff1 R S
    // -> iff1 Q S -> iff1 P R
    {
        Expression *T = init_var_expression("T", init_type_expression());
        Expression *P = init_var_expression(
            "P", init_arrow_expression(T, init_prop_expression()));
        Expression *Q = init_var_expression(
            "Q", init_arrow_expression(T, init_prop_expression()));
        Expression *R = init_var_expression(
            "R", init_arrow_expression(T, init_prop_expression()));
        Expression *S = init_var_expression(
            "S", init_arrow_expression(T, init_prop_expression()));

        Expression *iff1_P_Q = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), Q);
        Expression *iff1_R_S = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), R), S);
        Expression *iff1_Q_S = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), Q), S);
        Expression *iff1_P_R = init_app_expression(
            init_app_expression(init_app_expression(iff1, T), P), R);

        iff1_cong = init_var_expression(
            "iff1_cong",
            init_forall_expression(
                T, init_forall_expression(
                       P, init_forall_expression(
                              Q, init_forall_expression(
                                     R, init_forall_expression(
                                            S, init_arrow_expression(
                                                   iff1_P_Q,
                                                   init_arrow_expression(
                                                       iff1_R_S,
                                                       init_arrow_expression(
                                                           iff1_Q_S,
                                                           iff1_P_R)))))))));
    }

    return context_insert_n(c, 6, iff1, iff1_refl, iff1_sym, iff1_trans,
                            iff1_subst, iff1_cong);
}

Context *init_true(Context *c) {
    True = init_var_expression("True", init_prop_expression());
    I = init_var_expression("I", True);
    return context_insert_n(c, 2, True, I);
}

Expression *iff(Expression *A, Expression *B) {
    // iff = fun A B : Prop => (A -> B) /\ (B -> A)
    if (get_expression_type(A) != init_prop_expression() ||
        get_expression_type(B) != init_prop_expression()) {
        fprintf(stderr,
                "Error: iff requires both arguments to be of type Prop.\n");
        exit(EXIT_FAILURE);
    }

    Expression *A_B = init_arrow_expression(A, B);
    Expression *B_A = init_arrow_expression(B, A);
    return init_app_expression(init_app_expression(and, A_B), B_A);
}

Expression *impl1(Expression *T, Expression *P, Expression *Q) {
    // impl1 {T: Type} (P Q: T -> Prop) := forall x, P x -> Q x.
    if (T->type != TYPE_EXPRESSION) {
        fprintf(stderr, "Error: impl1 requires T to be a type\n");
        exit(EXIT_FAILURE);
    }

    Expression *x = init_var_expression("x", T);
    Expression *impl1_body = init_arrow_expression(init_app_expression(P, x),
                                                   init_app_expression(Q, x));
    return init_forall_expression(x, impl1_body);
}

// Expression *iff1(Expression *T, Expression *P, Expression *Q) {
//   // iff1 {T: Type} (P Q: T -> Prop) := forall x, P x <-> Q x.
//   if (get_expression_type(T) != init_type_expression() &&
//   get_expression_type(T) != init_prop_expression()) {
//     fprintf(stderr, "Error: iff1 requires T to be a type.\n");
//     exit(EXIT_FAILURE);
//   }

//   Expression *x = init_var_expression("x", T);
//   Expression *P_x = init_app_expression(P, x);
//   Expression *Q_x = init_app_expression(Q, x);
//   return iff(P_x, Q_x);
// }

Expression *and1(Expression *T, Expression *P, Expression *Q) {
    // and1 {T: Type} (P Q: T -> Prop) := fun x, P x /\ Q x.
    if (T->type != TYPE_EXPRESSION) {
        fprintf(stderr, "Error: and1 requires T to be a type.\n");
        exit(EXIT_FAILURE);
    }

    Expression *x = init_var_expression("x", T);
    Expression *P_x = init_app_expression(P, x);
    Expression *Q_x = init_app_expression(Q, x);
    Expression *and1_body =
        init_app_expression(init_app_expression(and, P_x), Q_x);

    return init_forall_expression(x, and1_body);
}

Expression *or1(Expression *T, Expression *P, Expression *Q) {
    // or1 {T: Type} (P Q: T -> Prop) := fun x, P x \/ Q x.
    if (T->type != TYPE_EXPRESSION) {
        fprintf(stderr, "Error: or1 requires T to be a type.\n");
        exit(EXIT_FAILURE);
    }

    Expression *x = init_var_expression("x", T);
    Expression *P_x = init_app_expression(P, x);
    Expression *Q_x = init_app_expression(Q, x);
    Expression *or1_body =
        init_app_expression(init_app_expression(or, P_x), Q_x);
    return init_forall_expression(x, or1_body);
}

Expression *ex1(Expression *A, Expression *B, Expression *P) {
    // ex1 {A B} (P : A -> B -> Prop) := fun (b:B) => exists a:A, P a b.
    if (A->type != TYPE_EXPRESSION || B->type != TYPE_EXPRESSION) {
        fprintf(stderr, "Error: ex1 requires A and B to be types.\n");
        exit(EXIT_FAILURE);
    }

    Expression *b = init_var_expression("b", B);
    Expression *a = init_var_expression("a", A);
    Expression *P_ab = init_app_expression(init_app_expression(P, a), b);
    Expression *ex1_body = init_app_expression(
        init_app_expression(ex, A),
        init_forall_expression(
            b, init_arrow_expression(
                   P_ab, init_app_expression(init_app_expression(ex, A), P))));
    return ex1_body;
}

Context *init_option(Context *c) {
    option = init_var_expression(
        "option",
        init_arrow_expression(init_type_expression(), init_type_expression()));

    Expression *A1 = init_var_expression("A", init_type_expression());
    option_some = init_var_expression(
        "option_some",
        init_forall_expression(
            A1, init_arrow_expression(A1, init_app_expression(option, A1))));

    Expression *A2 = init_var_expression("A", init_type_expression());
    option_none = init_var_expression(
        "option_none",
        init_forall_expression(A2, init_app_expression(option, A2)));

    return context_insert_n(c, 3, option, option_some, option_none);
}

Context *init_partial_map(Context *c) {
    partial_map = init_var_expression(
        "partial_map",
        init_arrow_expression(init_type_expression(),
                              init_arrow_expression(init_type_expression(),
                                                    init_type_expression())));

    Expression *A1 = init_var_expression("A", init_type_expression());
    Expression *B1 = init_var_expression("B", init_type_expression());
    partial_map_empty = init_var_expression(
        "partial_map_empty",
        init_forall_expression(
            A1, init_forall_expression(
                    B1, init_app_expression(
                            init_app_expression(partial_map, A1), B1))));

    Expression *A2 = init_var_expression("A", init_type_expression());
    Expression *B2 = init_var_expression("B", init_type_expression());
    partial_map_get = init_var_expression(
        "partial_map_get",
        init_forall_expression(
            A2, init_forall_expression(
                    B2, init_arrow_expression(
                            init_app_expression(
                                init_app_expression(partial_map, A2), B2),
                            init_arrow_expression(
                                A2, init_app_expression(option, B2))))));

    Expression *A3 = init_var_expression("A", init_type_expression());
    Expression *B3 = init_var_expression("B", init_type_expression());
    partial_map_put = init_var_expression(
        "partial_map_put",
        init_forall_expression(
            A3,
            init_forall_expression(
                B3,
                init_arrow_expression(
                    init_app_expression(init_app_expression(partial_map, A3),
                                        B3),
                    init_arrow_expression(
                        A3, init_arrow_expression(
                                B3, init_app_expression(
                                        init_app_expression(partial_map, A3),
                                        B3)))))));

    Expression *A4 = init_var_expression("A", init_type_expression());
    Expression *B4 = init_var_expression("B", init_type_expression());
    partial_map_remove = init_var_expression(
        "partial_map_remove",
        init_forall_expression(
            A4, init_forall_expression(
                    B4, init_arrow_expression(
                            init_app_expression(
                                init_app_expression(partial_map, A4), B4),
                            init_arrow_expression(
                                A4, init_app_expression(option, B4))))));

    // map.get_put_same : forall (key value : Type) (map : map key value)
    // (k : key) (v : value), eq (map.get (map.put m k v) k) (Some v)
    Expression *A5 = init_var_expression("A", init_type_expression());
    Expression *B5 = init_var_expression("B", init_type_expression());
    Expression *map5 = init_var_expression(
        "map", init_app_expression(init_app_expression(partial_map, A5), B5));
    Expression *k5 = init_var_expression("k", A5);
    Expression *v5 = init_var_expression("v", B5);
    Expression *sub5_1 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(partial_map_put, A5),
                                    B5),
                map5),
            k5),
        v5);
    Expression *sub5_2 = init_app_expression(
        init_app_expression(
            init_app_expression(init_app_expression(partial_map_get, A5), B5),
            sub5_1),
        k5);

    partial_map_get_put_same = init_var_expression(
        "partial_map_get_put_same",
        init_forall_expression(
            A5, init_forall_expression(
                    B5, init_forall_expression(
                            map5,
                            init_forall_expression(
                                k5, init_forall_expression(
                                        v5, init_app_expression(
                                                init_app_expression(
                                                    init_app_expression(
                                                        eq, init_app_expression(
                                                                option, B5)),
                                                    sub5_2),
                                                init_app_expression(
                                                    init_app_expression(
                                                        option_some, B5),
                                                    v5))))))));

    // partial_map_get_put_diff : forall (A B : Type) (map : partial_map A B) (k
    // : A) (v : B)
    //  (k' : A), not (eq A k k') ->
    //     eq (option B) (partial_map_get A B (partial_map_put A B map k' v) k)
    //                   (partial_map_get A B map k)
    Expression *A6 = init_var_expression("A", init_type_expression());
    Expression *B6 = init_var_expression("B", init_type_expression());
    Expression *map6 = init_var_expression(
        "map", init_app_expression(init_app_expression(partial_map, A6), B6));
    Expression *k6 = init_var_expression("k", A6);
    Expression *v6 = init_var_expression("v", B6);
    Expression *kp6 = init_var_expression("k'", A6);
    Expression *H6 = init_app_expression(
        not, init_app_expression(
                 init_app_expression(init_app_expression(eq, A6), k6), kp6));
    Expression *sub6_1 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(partial_map_put, A6),
                                    B6),
                map6),
            kp6),
        v6);
    Expression *sub6_2 = init_app_expression(
        init_app_expression(
            init_app_expression(init_app_expression(partial_map_get, A6), B6),
            sub6_1),
        k6);
    Expression *sub6_3 = init_app_expression(
        init_app_expression(
            init_app_expression(init_app_expression(partial_map_get, A6), B6),
            map6),
        k6);

    partial_map_get_put_diff = init_var_expression(
        "partial_map_get_put_diff",
        init_forall_expression(
            A6,
            init_forall_expression(
                B6,
                init_forall_expression(
                    map6,
                    init_forall_expression(
                        k6,
                        init_forall_expression(
                            v6, init_forall_expression(
                                    kp6,
                                    init_arrow_expression(
                                        H6, init_app_expression(
                                                init_app_expression(
                                                    init_app_expression(
                                                        eq, init_app_expression(
                                                                option, B6)),
                                                    sub6_2),
                                                sub6_3)))))))));

    Expression *A7 = init_var_expression("A", init_type_expression());
    Expression *B7 = init_var_expression("B", init_type_expression());
    Expression *map7 = init_var_expression(
        "map", init_app_expression(init_app_expression(partial_map, A7), B7));
    Expression *k7 = init_var_expression("k", A7);
    Expression *v7_1 = init_var_expression("v1", B7);
    Expression *v7_2 = init_var_expression("v2", B7);
    Expression *sub7_1 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(partial_map_put, A7),
                                    B7),
                map7),
            k7),
        v7_1);
    Expression *sub7_2 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(partial_map_put, A7),
                                    B7),
                sub7_1),
            k7),
        v7_2);
    Expression *sub7_3 = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(init_app_expression(partial_map_put, A7),
                                    B7),
                map7),
            k7),
        v7_2);

    partial_map_put_put_same = init_var_expression(
        "partial_map_put_put_same",
        init_forall_expression(
            A7,
            init_forall_expression(
                B7,
                init_forall_expression(
                    map7,
                    init_forall_expression(
                        k7,
                        init_forall_expression(
                            v7_1,
                            init_forall_expression(
                                v7_2, init_app_expression(
                                          init_app_expression(
                                              init_app_expression(
                                                  eq, init_app_expression(
                                                          init_app_expression(
                                                              partial_map, A7),
                                                          B7)),
                                              sub7_2),
                                          sub7_3))))))));

    return context_insert_n(
        c, 7, partial_map, partial_map_empty, partial_map_get, partial_map_put,
        partial_map_remove, partial_map_get_put_same, partial_map_put_put_same);
}

Context *init_nat(Context *c) {
    if (!nat) {
        nat = init_var_expression("nat", init_type_expression());
    }
    return context_insert_n(c, 1, nat);
}

Context *init_eq(Context *c) {
    // defining the eq type. eq : forall A : Type, A -> A -> Prop
    Expression *prop = init_prop_expression();
    Expression *type = init_type_expression();
    Expression *A = init_var_expression("A", type);
    Expression *eq_ty = init_forall_expression(
        A, init_arrow_expression(A, init_arrow_expression(A, prop)));
    if (!eq) {
        eq = init_var_expression("eq", eq_ty);
    }

    // defining the refl type. eq_refl : forall (B : Type) (x : B), eq B x x.
    Expression *B = init_var_expression("B", type);
    Expression *x = init_var_expression("x", B);
    Expression *refl_ty = init_forall_expression(
        B, init_forall_expression(
               x, init_app_expression(
                      init_app_expression(init_app_expression(eq, B), x), x)));
    if (!eq_refl) {
        eq_refl = init_var_expression("eq_refl", refl_ty);
    }

    // eq_sym : forall (A : Type) (x y : A), eq A x y -> eq A y x.
    Expression *A_sym = init_var_expression("A", init_type_expression());
    Expression *x_sym = init_var_expression("x", A_sym);
    Expression *y_sym = init_var_expression("y", A_sym);
    Expression *H_sym = init_app_expression(
        init_app_expression(init_app_expression(eq, A_sym), x_sym), y_sym);
    Expression *C_sym = init_app_expression(
        init_app_expression(init_app_expression(eq, A_sym), y_sym), x_sym);
    Expression *sym_ty = init_forall_expression(
        A_sym, init_forall_expression(
                   x_sym, init_forall_expression(
                              y_sym, init_arrow_expression(H_sym, C_sym))));
    if (!eq_sym) {
        eq_sym = init_var_expression("eq_sym", sym_ty);
    }

    // eq_subst: forall (P Q : Prop) (_: eq Prop P Q) (_: Q), P
    Expression *P3 = init_var_expression("P", init_prop_expression());
    Expression *Q3 = init_var_expression("Q", init_prop_expression());
    Expression *H3_1 = init_var_expression(
        "_", init_app_expression(
                 init_app_expression(
                     init_app_expression(eq, init_prop_expression()), P3),
                 Q3));
    Expression *H3_2 = init_var_expression("_", Q3);
    Expression *subst_ty = init_forall_expression(
        P3, init_forall_expression(
                Q3, init_forall_expression(H3_1,
                                           init_forall_expression(H3_2, P3))));
    if (!eq_subst) {
        eq_subst = init_var_expression("eq_subst", subst_ty);
    }

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
    Expression *app_cong_H1 = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_arrow_expression(A, B)), f),
        g);

    // app_cong_H2 = eq A x y
    Expression *app_cong_H2 = init_app_expression(
        init_app_expression(init_app_expression(eq, A), x), y);

    // app_cong_concl = eq B (f x) (g y)
    Expression *f_x = init_app_expression(f, x);
    Expression *g_y = init_app_expression(g, y);
    Expression *app_cong_concl = init_app_expression(
        init_app_expression(init_app_expression(eq, B), f_x), g_y);

    Expression *app_cong_body = init_arrow_expression(
        app_cong_H1, init_arrow_expression(app_cong_H2, app_cong_concl));

    // app_cong : forall (A B: Type) (f g : A -> B) (x y : A), f = g -> x = y ->
    // f x = g y
    Expression *app_cong_ty = init_forall_expression(
        A,
        init_forall_expression(
            B,
            init_forall_expression(
                f, init_forall_expression(
                       g, init_forall_expression(
                              x, init_forall_expression(y, app_cong_body))))));
    if (!app_cong) {
        app_cong = init_var_expression("app_cong", app_cong_ty);
    }

    return context_insert_n(c, 1, app_cong);
}

Context *init_eq_trans(Context *c) {
    // defining the eq_trans type
    Expression *A = init_var_expression("A", init_type_expression());
    Expression *x = init_var_expression("x", A);
    Expression *y = init_var_expression("y", A);
    Expression *z = init_var_expression("z", A);

    Expression *eq_trans_H1 = init_app_expression(
        init_app_expression(init_app_expression(eq, A), x), y);
    Expression *eq_trans_H2 = init_app_expression(
        init_app_expression(init_app_expression(eq, A), y), z);
    Expression *eq_trans_concl = init_app_expression(
        init_app_expression(init_app_expression(eq, A), x), z);

    Expression *eq_trans_body = init_arrow_expression(
        eq_trans_H1, init_arrow_expression(eq_trans_H2, eq_trans_concl));

    Expression *eq_trans_ty = init_forall_expression(
        A, init_forall_expression(
               x, init_forall_expression(
                      y, init_forall_expression(z, eq_trans_body))));

    if (!eq_trans) {
        eq_trans = init_var_expression("eq_trans", eq_trans_ty);
    }

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
    Expression *H_ty = init_forall_expression(
        x, init_app_expression(init_app_expression(init_app_expression(eq, B),
                                                   init_app_expression(f, x)),
                               init_app_expression(g, x)));
    Expression *H = init_var_expression("_", H_ty);

    Expression *lambda_extensionality_concl = init_app_expression(
        init_app_expression(
            init_app_expression(eq, init_arrow_expression(A, B)), f),
        g);

    Expression *lambda_extensionality_ty = init_forall_expression(
        A, init_forall_expression(
               B, init_forall_expression(
                      f, init_forall_expression(
                             g, init_forall_expression(
                                    H, lambda_extensionality_concl)))));

    if (!lambda_extensionality)
        lambda_extensionality = init_var_expression("lambda_extensionality",
                                                    lambda_extensionality_ty);

    return context_insert_n(c, 1, lambda_extensionality);
}

Context *init_add(Context *c) {
    // Define addition. add : nat -> (nat -> nat).
    Expression *add_ty =
        init_arrow_expression(nat, init_arrow_expression(nat, nat));
    if (!add) {
        add = init_var_expression("add", add_ty);
    }
    if (!O) {
        O = init_var_expression("O", nat);
    }

    // Axiomize Lemma add_r_O : forall (n : nat), eq nat ((add n) O) n.
    Expression *n = init_var_expression("n", nat);
    Expression *add_r_O_ty = init_forall_expression(
        n, init_app_expression(
               init_app_expression(
                   init_app_expression(eq, nat),
                   init_app_expression(init_app_expression(add, n), O)),
               n));
    if (!add_r_O) {
        add_r_O = init_var_expression("add_r_O", add_r_O_ty);
    }

    return context_insert_n(c, 3, add, O, add_r_O);
}

Context *init_temporary(Context *ctx) {
    Expression *f_ty = init_arrow_expression(nat, nat);
    Expression *a_ty = nat;

    if (!f) {
        f = init_var_expression("f", f_ty);
    }
    if (!a) {
        a = init_var_expression("a", a_ty);
    }
    if (!b) {
        b = init_var_expression("b", a_ty);
    }

    Expression *fa_a_equality =
        init_app_expression(init_app_expression(init_app_expression(eq, nat),
                                                init_app_expression(f, a)),
                            a);

    if (!eq_fa_a) {
        eq_fa_a = init_var_expression("eq_fa_a", fa_a_equality);
    }

    Expression *g_ty = init_arrow_expression(nat, nat);
    if (!g) {
        g = init_var_expression("g", g_ty);
    }

    Expression *h_ty =
        init_arrow_expression(nat, init_arrow_expression(nat, nat));
    if (!h) {
        h = init_var_expression("h", h_ty);
    }
    if (!c) {
        c = init_var_expression("c", nat);
    }

    Expression *x = init_var_expression("x", nat);

    Expression *hxx = init_app_expression(init_app_expression(h, x), x);

    Expression *hxx_x_equality = init_app_expression(
        init_app_expression(init_app_expression(eq, nat), hxx), x);

    Expression *hxx_x_ty = init_forall_expression(x, hxx_x_equality);
    if (!eq_hxx_x) {
        eq_hxx_x = init_var_expression("eq_hxx_x", hxx_x_ty);
    }

    return context_insert_n(ctx, 8, f, a, b, eq_fa_a, g, h, c, eq_hxx_x);
}

void init_globals() {
    Context *c = context_create_empty();

    c = init_and(c);
    c = init_or(c);
    c = init_not(c);
    c = init_ex(c);
    c = init_iff1(c);
    c = init_true(c);
    c = init_option(c);
    c = init_nat(c);
    c = init_eq(c);
    c = init_app_cong(c);
    c = init_eq_trans(c);
    c = init_lambda_extensionality(c);
    c = init_partial_map(c);
    c = init_add(c);

    std_lib_ctx = c;

    init_temporary(c);
}