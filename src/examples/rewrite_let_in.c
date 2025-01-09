#include "rewrite_let_in.h"

RewriteProof *rewrite_let_in__sharing(int n_depth) {
  init_globals();

  Context *base_ctx = ctx_with_axioms;
  // Define addition. add : nat -> (nat -> nat).
  Expression *add = init_var_expression("add");
  Expression *add_ty = init_arrow_expression(
      base_ctx, nat, init_arrow_expression(base_ctx, nat, nat));
  Context *with_add = context_insert(base_ctx, add, add_ty);

  // Define O : nat.
  Expression *O = init_var_expression("O");
  Expression *O_ty = nat;
  Context *with_O = context_insert(with_add, O, O_ty);

  // Axiomize Lemma add_r_O : forall (n : nat), eq nat ((add n) O) n.
  Expression *add_r_O = init_var_expression("add_r_O");
  // Need to build the context which the lemma's type is valid under.
  Expression *n = init_var_expression("n");
  Expression *n_ty = nat;
  Context *lemma_ty_context = context_insert(with_O, n, n_ty);
  Expression *add_r_O_ty = init_forall_expression(
      lemma_ty_context,  // Forall (n: nat), ...
      init_app_expression(
          lemma_ty_context,
          init_app_expression(
              lemma_ty_context, init_app_expression(lemma_ty_context, eq, nat),
              init_app_expression(lemma_ty_context,
                                  init_app_expression(lemma_ty_context, add, n),
                                  O)),
          n));

  // Insert the lemma into our working context with_O.
  Context *with_lemma = context_insert(with_O, add_r_O, add_r_O_ty);

  // Now define LetIn : nat -> (nat -> nat) -> nat
  Expression *LetIn = init_var_expression("LetIn");
  Expression *LetIn_ty = init_arrow_expression(
      with_lemma, nat,
      init_arrow_expression(with_lemma,
                            init_arrow_expression(with_lemma, nat, nat), nat));
  Context *with_LetIn = context_insert(with_lemma, LetIn, LetIn_ty);

  // Want to build: fun (vn: nat) => vn + vn + 0,
  // i.e., fun (vn: nat) => (add ((add vn) vn)) 0
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "v%d", n_depth);
  Expression *vn = init_var_expression(buffer);
  Expression *vn_ty = nat;
  Context *curr_ctx = context_insert(with_LetIn, vn, vn_ty);
  Expression *curr_expr = init_lambda_expression(curr_ctx,
    init_app_expression(curr_ctx,
      init_app_expression(curr_ctx, add,
        init_app_expression(curr_ctx, init_app_expression(curr_ctx, add, vn), vn)),
      O));


  for (int i = n_depth - 1; i > 0; i--) {
    snprintf(buffer, sizeof(buffer), "v%d", i);
    Expression *vi = init_var_expression(buffer);
    Expression *vi_ty = nat;
    Context *curr_ctx = context_insert(with_LetIn, vi, vi_ty);
    Expression *subexpr = init_app_expression(curr_ctx,
      init_app_expression(curr_ctx, add,
        init_app_expression(curr_ctx, init_app_expression(curr_ctx, add, vi), vi)),
      O);
    curr_expr = init_lambda_expression(curr_ctx,
      init_app_expression(curr_ctx, 
        init_app_expression(curr_ctx, LetIn, subexpr), curr_expr));
  }

  Expression *v0 = init_var_expression("v0");
  Expression *v0_ty = nat;
  curr_ctx = context_insert(with_LetIn, v0, v0_ty); 
  Expression *subexpr = init_app_expression(curr_ctx,
      init_app_expression(curr_ctx, add,
        init_app_expression(curr_ctx, init_app_expression(curr_ctx, add, v0), v0)),
      O);
  curr_expr = init_app_expression(curr_ctx, init_app_expression(curr_ctx, LetIn, subexpr), curr_expr);

  return rewrite(curr_ctx, curr_expr, add_r_O);
}