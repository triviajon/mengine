#include "rewrite_addr0.h"

RewriteProof *rewrite_addr0__letin(int n_depth) {
  // LetIn : nat -> (nat -> nat) -> nat
  Expression *LetIn_ty = init_arrow_expression(
      nat, init_arrow_expression(init_arrow_expression(nat, nat), nat));
  Expression *LetIn = init_var_expression("LetIn", LetIn_ty);

  // fun (vn: nat) => (add ((add vn) vn)) 0
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "v%d", n_depth);
  Expression *vn = init_var_expression(buffer, nat);
  Expression *curr_expr = init_lambda_expression(vn, 
    init_app_expression(
      init_app_expression(add, 
        init_app_expression(
          init_app_expression(add, vn), vn)), O));

  for (int i = n_depth - 1; i > 0; i--) {
    snprintf(buffer, sizeof(buffer), "v%d", i);
    Expression *vi = init_var_expression(buffer, nat);
    Expression *subexpr = init_app_expression(
      init_app_expression(
        add, init_app_expression(init_app_expression(add, vi), vi)),
      O);
    curr_expr = init_lambda_expression(vi,
      init_app_expression(init_app_expression(LetIn, subexpr), curr_expr));
  }

  Expression *v0 = init_var_expression("v0", nat);
  Expression *subexpr = init_app_expression(
      init_app_expression(add,
                          init_app_expression(init_app_expression(add, v0), v0)),
      O);
  curr_expr = init_app_expression(
      init_app_expression(LetIn, subexpr), curr_expr);

  return rewrite(curr_expr, add_r_O);
}

Expression *build_vi(Expression *v0, int i) {
  if (i == 0) {
    return v0;
  } else {
    Expression *vim1_a = build_vi(v0, i - 1);
    Expression *vim1_b = build_vi(v0, i - 1);
    return init_app_expression(
      init_app_expression(add, 
        init_app_expression(init_app_expression(add, vim1_a), vim1_b)), 
    O);
  }
}

RewriteProof *rewrite_addr0__tree(int n_depth) {
  Expression *v0 = init_var_expression("v0", nat);
  Expression *vn = build_vi(v0, n_depth + 1);
  return rewrite(vn, add_r_O);
}

RewriteProof *rewrite_addr0__native(int n_depth) {
  // v0 : nat
  Expression *vnm1 = init_var_expression("v0", nat);

  for (int i = 1; i <= n_depth; i++) {
    // v_{n} = v_{n - 1} + v_{n - 1} + 0
    // make use of native sharing by both v_{n - 1} reference the same place
    Expression *doubled = init_app_expression(init_app_expression(add, vnm1), vnm1);
    vnm1 = init_app_expression(init_app_expression(add, doubled), O);
  }
  Expression *expr = init_app_expression(
    init_app_expression(add, 
      init_app_expression(init_app_expression(add, vnm1), vnm1)), 
  O);
  return rewrite(expr, add_r_O);
}