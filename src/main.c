#include <stdio.h>
#include <stdlib.h>

#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/inductive.h"
#include "kernel/utils.h"

#include "engine/tactics.h"
#include "engine/axiom.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <f_length> <g_wrap>\n", argv[0]);
    return 1;
  }

  int f_length = atoi(argv[1]);
  int g_wrap = atoi(argv[2]);

  init_globals();

  // Build (f ... (f a)) with `f_length` number of f's
  Expression *current_expr = a;
  for (int i = 0; i < f_length; i++) {
    current_expr = init_app_expression(g_f_a_ctx, f, current_expr);
  }

  if (g_wrap) {
    current_expr = init_app_expression(g_f_a_ctx, g, current_expr);
  }

  printf("Require Import Setoid Morphisms.\n");
  printf("%s\n", stringify_context2(g_f_a_ctx));
  printf("Declare Instance Equivalence_eq : Equivalence eq.\nInstance f_Proper : Proper (eq ==> eq) f := f_equal f.\nInstance f_Proper : Proper (eq ==> eq) g := f_equal g.");
  RewriteProof *rw_pf = rewrite_example(current_expr);
  printf("\nCheck %s : eq (%s) (%s).\n", stringify_expression_with_let(rw_pf->equality_proof), stringify_expression(rw_pf->expr), stringify_expression(rw_pf->rewritten_expr));

  return 0;
}
