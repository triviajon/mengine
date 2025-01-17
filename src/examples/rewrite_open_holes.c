#include "rewrite_open_holes.h"

RewriteProof *rewrite_open_holes() { 
  init_globals();

  Expression *O = init_var_expression("O");
  Context *base_ctx = context_insert(g_f_a_ctx, O, nat); 
  Expression *multiply_var = init_var_expression("mul");
  Expression *multiply_ty = init_arrow_expression(base_ctx, nat, init_arrow_expression(base_ctx, nat, nat));
  base_ctx = context_insert(base_ctx, multiply_var, multiply_ty);

  Expression *mult_n_O = init_var_expression("mult_n_O");
  Expression *n = init_var_expression("n");
  Context *multi_n_O_ctx = context_insert(base_ctx, n, nat);
  Expression *mult_n_O_ty = init_forall_expression(multi_n_O_ctx, 
    init_app_expression(multi_n_O_ctx, 
      init_app_expression(multi_n_O_ctx, init_app_expression(multi_n_O_ctx, eq, nat), O), 
        init_app_expression(multi_n_O_ctx, init_app_expression(multi_n_O_ctx, multiply_var, n), O)));
  // mult_n_O : forall (n: nat), O = n * O
  Context *final_ctx = context_insert(base_ctx, mult_n_O, mult_n_O_ty);

  Expression *expr = init_app_expression(final_ctx, f, O);

  return rewrite(final_ctx, expr, mult_n_O);
}
