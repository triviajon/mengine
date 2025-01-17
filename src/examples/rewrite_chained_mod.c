#include "rewrite_chained_mod.h"

RewriteProof *rewrite_chained_mod(int n_depth) {
  init_globals();

  Context *base_ctx = ctx_with_axioms;
  // Define modulo. mod : nat -> (nat -> nat).
  Expression *mod = init_var_expression("mod");
  Expression *mod_ty = init_arrow_expression(
      base_ctx, nat, init_arrow_expression(base_ctx, nat, nat));
  Context *with_mod = context_insert(base_ctx, mod, mod_ty);

  // Axiomize Lemma mod_mod : forall a n : nat, eq nat (mod (mod a n) n) (mod a n)
  Expression *mod_mod = init_var_expression("mod_mod");
  // Need to build the context which the lemma's type is valid under.
  Expression *a = init_var_expression("a");
  Expression *n = init_var_expression("n");
  Context *lemma_ty_int_context = context_insert(with_mod, n, nat);
  Context *lemma_ty_context = context_insert(lemma_ty_int_context, a, nat);

  // subexpr0 = mod a n
  Expression *subexpr0 = init_app_expression(lemma_ty_context, init_app_expression(lemma_ty_context, mod, a), n);
  // subexpr1 = mod (subexpr0) n
  Expression *subexpr1 = init_app_expression(lemma_ty_context, init_app_expression(lemma_ty_context, mod, subexpr0), n);
  Expression *mod_mod_ty = init_forall_expression(
    lemma_ty_context,  // Forall (a: nat), 
    init_forall_expression(
      lemma_ty_int_context, // Forall (n: nat), 
        init_app_expression(lemma_ty_context, 
          init_app_expression(lemma_ty_context, 
          init_app_expression(lemma_ty_context, eq, nat), subexpr1), subexpr0)
    )
  );

  // Insert the lemma into our working context with mod.
  Context *with_lemma = context_insert(with_mod, mod_mod, mod_mod_ty);

  Expression *b = init_var_expression("b");
  Expression *p = init_var_expression("p");
  Context *curr_ctx = context_insert(context_insert(with_lemma, b, nat), p, nat);

  Expression *curr_expr = init_app_expression(curr_ctx, init_app_expression(curr_ctx, mod, b), p);
  for (int i = 1; i < n_depth; i++) {
    curr_expr = init_app_expression(curr_ctx, init_app_expression(curr_ctx, mod, curr_expr), p);
  }
  
  return rewrite(curr_ctx, curr_expr, mod_mod);
}