#include "rewrite_let_in.h"

RewriteProof *rewrite_let_in__sharing(int n_depth) {
  init_globals();

  // Define addition. add : nat -> (nat -> nat).
  Expression *add_ty =
      init_arrow_expression(nat, init_arrow_expression(nat, nat));
  Expression *add = init_var_expression("add", add_ty);

  // Define O : nat.
  Expression *O = init_var_expression("O", nat);

  // Axiomize Lemma add_r_O : forall (n : nat), eq nat ((add n) O) n.
  // Need to build the context which the lemma's type is valid under.
  Expression *n = init_var_expression("n", nat);
  Expression *add_r_O_ty = init_forall_expression(
      n, init_app_expression(
             init_app_expression(
                 init_app_expression(eq, nat),
                 init_app_expression(init_app_expression(add, n), O)),
             n));
  Expression *add_r_O = init_var_expression("add_r_O", add_r_O_ty);

  // Now define LetIn : nat -> (nat -> nat) -> nat
  Expression *LetIn_ty = init_arrow_expression(
      nat, init_arrow_expression(init_arrow_expression(nat, nat), nat));
  Expression *LetIn = init_var_expression("LetIn", LetIn_ty);

  // Want to build: fun (vn: nat) => vn + vn + 0,
  // i.e., fun (vn: nat) => (add ((add vn) vn)) 0
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "v%d", n_depth);
  Expression *vn = init_var_expression(buffer, nat);
  Expression *curr_expr = init_lambda_expression(
      vn, init_app_expression(
              init_app_expression(
                  add, init_app_expression(init_app_expression(add, vn), vn)),
              O));

  for (int i = n_depth - 1; i > 0; i--) {
    snprintf(buffer, sizeof(buffer), "v%d", i);
    Expression *vi = init_var_expression(buffer, nat);
    Expression *subexpr = init_app_expression(
        init_app_expression(
            add, init_app_expression(init_app_expression(add, vi), vi)),
        O);
    curr_expr = init_lambda_expression(
        vi,
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