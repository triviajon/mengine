#include "rewrite_open_holes.h"

RewriteProof *rewrite_open_holes() { 

  Expression *O = init_var_expression("O", nat);
  Expression *multiply_ty = init_arrow_expression(nat, init_arrow_expression(nat, nat));
  Expression *multiply_var = init_var_expression("mul", multiply_ty);

  Expression *n = init_var_expression("n", nat);
  Expression *mult_n_O_ty = init_forall_expression(n, 
    init_app_expression(init_app_expression(init_app_expression(eq, nat), O), 
        init_app_expression(init_app_expression(multiply_var, n), O)));
  // mult_n_O : forall (n: nat), O = n * O
  Expression *mult_n_O = init_var_expression("mult_n_O", mult_n_O_ty);

  Expression *expr = init_app_expression(f, O);

  return rewrite(expr, mult_n_O);
}
