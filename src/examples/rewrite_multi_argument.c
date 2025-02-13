#include "rewrite_multi_argument.h"

RewriteProof *rewrite_haa(int h_depth) {
  Expression *haa = init_app_expression(init_app_expression(h, a), a);
  Expression *current_expr = haa;

  for (int i = 0; i < h_depth; i++) {
    Expression *intermediate = init_app_expression(h, current_expr);
    current_expr = init_app_expression(intermediate, current_expr);
  }

  return rewrite(current_expr, eq_hxx_x);
}