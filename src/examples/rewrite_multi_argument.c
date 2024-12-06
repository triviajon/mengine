#include "rewrite_multi_argument.h"

RewriteProof *rewrite_haa(int h_depth) {
  init_globals();
  Expression *haa = init_app_expression(
      h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, a), a);
  Expression *current_expr = haa;

  for (int i = 0; i < h_depth; i++) {
    Expression *intermediate =
        init_app_expression(h_g_f_a_ctx, h, current_expr);
    current_expr = init_app_expression(h_g_f_a_ctx, intermediate, current_expr);
  }

  return rewrite(get_expression_context(current_expr), current_expr, eq_haa_a);
}
