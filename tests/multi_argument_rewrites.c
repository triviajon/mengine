#include <stdio.h>
#include <stdlib.h>

#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/utils.h"

#include "engine/tactics.h"
#include "engine/axiom.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <h_depth>\n", argv[0]);
    return 1;
  }

  init_globals();
  Expression *haa = init_app_expression(h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, a), a);
  Expression *current_expr = haa;

  int depth = atoi(argv[1]);
  for (int i = 0; i < depth; i++) {
    Expression *intermediate = init_app_expression(h_g_f_a_ctx, h, current_expr);
    current_expr = init_app_expression(h_g_f_a_ctx, intermediate, current_expr);
  }

  printf("Section T.\n");
  printf("%s\n", stringify_context2(h_g_f_a_ctx));
  RewriteProof *rw_pf = rewrite(get_expression_context(current_expr), current_expr, eq_haa_a);
  Expression *expr_ty = get_expression_type(get_expression_context(current_expr), current_expr);
  printf("\nCheck %s : eq (%s) (%s) (%s).\n",
         stringify_expression2(rw_pf->equality_proof),
         stringify_expression2(expr_ty), stringify_expression2(rw_pf->expr),
         stringify_expression2(rw_pf->rewritten_expr));
}
