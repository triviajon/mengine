#include "rewrite_chained_mod.h"

RewriteProof *rewrite_chained_mod(int n_depth) {

  // Define modulo. mod : nat -> (nat -> nat).
  Expression *mod_ty = init_arrow_expression(
      nat, init_arrow_expression(nat, nat));
  Expression *mod = init_var_expression("mod", mod_ty);

  // Axiomize Lemma mod_mod : forall a n : nat, eq nat (mod (mod a n) n) (mod a n)
  // Need to build the context which the lemma's type is valid under.
  Expression *a = init_var_expression("a", nat);
  Expression *n = init_var_expression("n", nat);
  
  // subexpr0 = mod a n
  Expression *subexpr0 = init_app_expression(init_app_expression(mod, a), n);
  // subexpr1 = mod (subexpr0) n
  Expression *subexpr1 = init_app_expression(init_app_expression(mod, subexpr0), n);
  Expression *mod_mod_ty = init_forall_expression(
    a,
    init_forall_expression(
      n,
        init_app_expression(init_app_expression(init_app_expression(eq, nat), subexpr1), subexpr0)
    )
  );
  Expression *mod_mod = init_var_expression("mod_mod", mod_mod_ty);

  Expression *b = init_var_expression("b", nat);
  Expression *p = init_var_expression("p", nat);

  Expression *curr_expr = init_app_expression(init_app_expression(mod, b), p);
  for (int i = 1; i < n_depth; i++) {
    curr_expr = init_app_expression(init_app_expression(mod, curr_expr), p);
  }
  
  return rewrite(curr_expr, mod_mod);
}