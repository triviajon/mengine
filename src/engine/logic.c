#include "logic.h"

// Given an expression e, builds the new expression of type (eq e e) using the
// eq_refl constructor of the inductive type "eq"
Expression *build_eq_refl(Context *ctx, Expression *e) {
  Expression *e_type = get_expression_type(ctx, e);
  return init_app_expression(ctx, init_app_expression(ctx, eq_refl, e_type),
                             e);
}

// Given expressions f_equality (representing equality between some f and f'),
// and x_equality (similar), returns an expression which says that the
// application (f x') is equal to (f' x)
Expression *build_app_cong(Context *app_context, RewriteProof *f_equality,
                          RewriteProof *x_equality) {
  Expression *f = f_equality->expr;
  Expression *fp = f_equality->rewritten_expr;
  Expression *eq_f_fp = f_equality->equality_proof;

  Expression *x = x_equality->expr;
  Expression *xp = x_equality->rewritten_expr;
  Expression *eq_x_xp = x_equality->equality_proof;

  Expression *fx = init_app_expression(app_context, f, x);
  Expression *fp_xp = init_app_expression(app_context, fp, xp);
  
  Expression *x_ty = get_expression_type(app_context, x);
  Expression *fx_ty = get_expression_type(app_context, fx);

  return init_app_expression(app_context, init_app_expression(app_context,
    init_app_expression(app_context,
        init_app_expression(app_context,
            init_app_expression(app_context,
              init_app_expression(app_context,
                init_app_expression(app_context,
                    init_app_expression(app_context, 
                      app_cong, x_ty), fx_ty),
                f), fp),
            x), xp),
    eq_f_fp), eq_x_xp);
}

// Given expressions x_y_equality (representing equality between some x and y)
// and y_z_equality (representing equality between some y and z), returns an
// expression representing the equality between x and z, i.e., (eq x z).
Expression *build_eq_trans(Context *app_context, RewriteProof *x_y_equality,
                           RewriteProof *y_z_equality) {
  Expression *x = x_y_equality->expr;
  Expression *y = x_y_equality->rewritten_expr;
  Expression *x_y = x_y_equality->equality_proof;
  // can assert y's are the same
  Expression *z = y_z_equality->rewritten_expr;
  Expression *y_z = y_z_equality->equality_proof;

  return init_app_expression(
      app_context,
      init_app_expression(
          app_context,
          init_app_expression(
              app_context,
              init_app_expression(app_context,
                                  init_app_expression(app_context, eq_trans, x),
                                  y),
              z),
          x_y),
      y_z);
}


Expression *build_lambda_extensionality(Context *ctx, Expression *A, Expression *B, Expression *f, Expression *g, Expression *eq_paramaterized) {
  return init_app_expression(ctx, 
    init_app_expression(ctx, 
      init_app_expression(ctx, 
        init_app_expression(ctx, 
          init_app_expression(ctx, lambda_extensionality, A), B), f), g), eq_paramaterized);
}
