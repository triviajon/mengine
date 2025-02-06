#include "rewrite_single_argument.h"

RewriteProof *rewrite_gfa(int f_length, int g_wrap) {
  init_globals();
  Expression *current_expr = b;
  for (int i = 0; i < f_length; i++) {
    current_expr = init_app_expression(f, current_expr);
  }

  if (g_wrap) {
    current_expr = init_app_expression(g, current_expr);
  }

  return rewrite(current_expr, eq_fa_a);
}
