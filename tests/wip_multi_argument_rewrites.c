#include <stdio.h>
#include <stdlib.h>

#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/inductive.h"
#include "kernel/utils.h"

#include "engine/tactics.h"
#include "engine/axiom.h"

int main(int argc, char *argv[]) {
  init_globals();
  Expression *h_a_a = init_app_expression(h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, a), a);
  Expression *expr = init_app_expression(h_g_f_a_ctx, init_app_expression(h_g_f_a_ctx, h, h_a_a), h_a_a);
  printf("%s\n", stringify_context2(h_g_f_a_ctx));
  RewriteProof *rw_pf = rewrite(h_a_a, eq_haa_a);
  printf("\nCheck %s : eq (%s) (%s).\n", stringify_expression(rw_pf->equality_proof), stringify_expression(rw_pf->expr), stringify_expression(rw_pf->rewritten_expr));
}
