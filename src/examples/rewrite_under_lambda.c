#include "rewrite_under_lambda.h"

RewriteProof *rewrite_lambda_f_x() {
  init_globals();

  Expression *x = init_var_expression("x");
  Context *x_ctx = context_insert(g_f_a_ctx, x, nat);
  Expression *expr = init_lambda_expression(x_ctx, init_app_expression(x_ctx, f, x));

  return rewrite(x_ctx, expr, eq_fa_a);
}
