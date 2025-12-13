#include "core.h"

Expression *Equivalence = NULL;
Expression *Build_Equivalence = NULL;
Expression *Equiv_App_Cong = NULL;

static Context *init_equivalence(Context *c) {
    // Equivalence : forall (A : Type) (R : A -> A -> Prop), Prop
    Expression *A1 = init_var_expression_wc("A", init_type_expression(), c);
    Context *c1 = context_insert(c, A1);

    Expression *R1 = init_var_expression_wc("R",
        init_arrow_expression(A1, init_arrow_expression(A1, init_prop_expression())), c1);
    Context *c2 = context_insert(c1, R1);

    Expression *equiv_type = init_forall_expression_wc(R1, init_prop_expression(), c2);
    equiv_type = init_forall_expression_wc(A1, equiv_type, c1);

    Equivalence = init_var_expression_wc("Equivalence", equiv_type, c);

    // Build_Equivalence : forall (A : Type) (R : A -> A -> Prop),
    //   (forall (x: A), R x x) ->
    //   (forall (x y: A), R x y -> R y x) ->
    //   (forall (x y z: A), R x y -> R y z -> R x z) ->
    //   Equivalence A R

    Context *build_ctx = context_insert(c, Equivalence);

    Expression *A = init_var_expression_wc("A", init_type_expression(), build_ctx);
    Context *ctx_A = context_insert(build_ctx, A);

    Expression *R = init_var_expression_wc("R",
        init_arrow_expression(A, init_arrow_expression(A, init_prop_expression())), ctx_A);
    Context *ctx_R = context_insert(ctx_A, R);

    Expression *refl_type;
    {
        // Reflexivity: forall (x: A), R x x
        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = context_insert(ctx_R, x);
        Expression *R_x_x = init_app_expression_wc(init_app_expression_wc(R, x, ctx_x), x, ctx_x);
        refl_type = init_forall_expression_wc(x, R_x_x, ctx_x);
    }

    Expression *sym_type;
    {
        // Symmetry: forall (x y: A), R x y -> R y x
        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = context_insert(ctx_R, x);
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = context_insert(ctx_x, y);
        Expression *R_x_y = init_app_expression_wc(init_app_expression_wc(R, x, ctx_y), y, ctx_y);
        Expression *R_y_x = init_app_expression_wc(init_app_expression_wc(R, y, ctx_y), x, ctx_y);
        Expression *sym_body = init_arrow_expression(R_x_y, R_y_x);
        sym_type = init_forall_expression_wc(y, sym_body, ctx_y);
        sym_type = init_forall_expression_wc(x, sym_type, ctx_x);
    }

    Expression *trans_type;
    {
        // Transitivity: forall (x y z: A), R x y -> R y z -> R x z
        Expression *x = init_var_expression_wc("x", A, ctx_R);
        Context *ctx_x = context_insert(ctx_R, x);
        Expression *y = init_var_expression_wc("y", A, ctx_x);
        Context *ctx_y = context_insert(ctx_x, y);
        Expression *z = init_var_expression_wc("z", A, ctx_y);
        Context *ctx_z = context_insert(ctx_y, z);
        Expression *R_x_y = init_app_expression_wc(init_app_expression_wc(R, x, ctx_z), y, ctx_z);
        Expression *R_y_z = init_app_expression_wc(init_app_expression_wc(R, y, ctx_z), z, ctx_z);
        Expression *R_x_z = init_app_expression_wc(init_app_expression_wc(R, x, ctx_z), z, ctx_z);
        Expression *trans_body = init_arrow_expression(R_x_y, init_arrow_expression(R_y_z, R_x_z));
        trans_type = init_forall_expression_wc(z, trans_body, ctx_z);
        trans_type = init_forall_expression_wc(y, trans_type, ctx_y);
        trans_type = init_forall_expression_wc(x, trans_type, ctx_x);
    }

    // Build conclusion: Equivalence A R
    Expression *equiv_applied = init_app_expression_wc(init_app_expression_wc(Equivalence, A, ctx_R), R, ctx_R);

    // Build_Equivalence type: forall A R, refl_type -> sym_type -> trans_type -> Equivalence A R
    Expression *build_body = init_arrow_expression(refl_type,
        init_arrow_expression(sym_type,
            init_arrow_expression(trans_type, equiv_applied)));
    Expression *build_type = init_forall_expression_wc(R, build_body, ctx_R);
    build_type = init_forall_expression_wc(A, build_type, ctx_A);

    Build_Equivalence = init_var_expression_wc("Build_Equivalence", build_type, build_ctx);

    // Equiv_App_Cong : forall (A : Type) (R : A -> A -> Prop) (equiv : Equivalence A R),
    //   forall (f : A -> A) (x x' : A), R x x' -> R (f x) (f x')
    Context *cong_ctx = context_insert_n(c, 2, Equivalence, Build_Equivalence);

    Expression *A_cong = init_var_expression_wc("A", init_type_expression(), cong_ctx);
    Context *ctx_A_cong = context_insert(cong_ctx, A_cong);

    Expression *R_cong = init_var_expression_wc("R",
        init_arrow_expression(A_cong, init_arrow_expression(A_cong, init_prop_expression())), ctx_A_cong);
    Context *ctx_R_cong = context_insert(ctx_A_cong, R_cong);

    Expression *equiv_cong = init_var_expression_wc("equiv",
        init_app_expression_wc(init_app_expression_wc(Equivalence, A_cong, ctx_R_cong), R_cong, ctx_R_cong),
        ctx_R_cong);
    Context *ctx_equiv_cong = context_insert(ctx_R_cong, equiv_cong);

    Expression *f_cong = init_var_expression_wc("f",
        init_arrow_expression(A_cong, A_cong), ctx_equiv_cong);
    Context *ctx_f_cong = context_insert(ctx_equiv_cong, f_cong);

    Expression *x_cong = init_var_expression_wc("x", A_cong, ctx_f_cong);
    Context *ctx_x_cong = context_insert(ctx_f_cong, x_cong);

    Expression *xp_cong = init_var_expression_wc("x'", A_cong, ctx_x_cong);
    Context *ctx_xp_cong = context_insert(ctx_x_cong, xp_cong);

    // R x x'
    Expression *R_x_xp = init_app_expression_wc(init_app_expression_wc(R_cong, x_cong, ctx_xp_cong), xp_cong, ctx_xp_cong);

    // R (f x) (f x')
    Expression *fx = init_app_expression_wc(f_cong, x_cong, ctx_xp_cong);
    Expression *fxp = init_app_expression_wc(f_cong, xp_cong, ctx_xp_cong);
    Expression *R_fx_fxp = init_app_expression_wc(init_app_expression_wc(R_cong, fx, ctx_xp_cong), fxp, ctx_xp_cong);

    // R x x' -> R (f x) (f x')
    Expression *cong_body = init_arrow_expression(R_x_xp, R_fx_fxp);

    // forall (f : A -> A) (x x' : A), R x x' -> R (f x) (f x')
    Expression *cong_type = init_forall_expression_wc(xp_cong, cong_body, ctx_xp_cong);
    cong_type = init_forall_expression_wc(x_cong, cong_type, ctx_x_cong);
    cong_type = init_forall_expression_wc(f_cong, cong_type, ctx_f_cong);
    cong_type = init_forall_expression_wc(equiv_cong, cong_type, ctx_equiv_cong);
    cong_type = init_forall_expression_wc(R_cong, cong_type, ctx_R_cong);
    cong_type = init_forall_expression_wc(A_cong, cong_type, ctx_A_cong);

    Equiv_App_Cong = init_var_expression_wc("Equiv_App_Cong", cong_type, cong_ctx);

    return context_insert_n(c, 3, Equivalence, Build_Equivalence, Equiv_App_Cong);
}

void init_core(Context **ctx) {
    if (*ctx == NULL) {
        *ctx = context_create_empty();
    }
    *ctx = init_equivalence(*ctx);
}