#include "logic.h"

// Given an expression e, builds the new expression of type (eq e e) using the
// eq_refl constructor of the inductive type "eq"
Expression *build_eq_refl(Expression *e) {
  Context *e_ctx = get_expression_context(e);
  return init_app_expression(e_ctx, eq_refl, e);
}

// Given expressions f, and x_equality (representing equality between some x and
// x'), returns an expression which says that the application (f x') is equal to
// (f x)
Expression *build_f_equal(Context *app_context, Expression *f,
                          RewriteProof *x_equality) {
  Expression *x = x_equality->expr;
  Expression *x_p = x_equality->rewritten_expr;
  Expression *eq_x_xp = x_equality->equality_proof;
  return init_app_expression(
      app_context,
      init_app_expression(
          app_context,
          init_app_expression(app_context,
                              init_app_expression(app_context, f_equal, f), x),
          x_p),
      eq_x_xp);
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
