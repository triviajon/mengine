#include <stdio.h>
#include <stdlib.h>

#include "kernel/context.h"
#include "kernel/expression.h"
#include "kernel/inductive.h"
#include "kernel/utils.h"

#include "engine/tactics.h"
#include "engine/axiom.h"

int main() {
  init_globals();

  // Build the expression (f a) under the context which f: nat -> nat, a : nat, and things like nat, eq, eq_refl, f_equal, and eq_trans are defined.
  Expression *f_a = init_app_expression(g_f_a_ctx, f, a);

  // Build the expression f (f a) under the same context as above...
  Expression *f_f_a = init_app_expression(g_f_a_ctx, f, f_a);

  // Build the expression (g (f (f a))) under a similar context as above, but also extended with g: nat -> nat.
  Expression *g_f_f_a = init_app_expression(g_f_a_ctx, g, f_f_a);
  // // Attempt to rewrite (g (f (f a))) using an example rewriter which has the lemma "(f a) = a" built-in (but NOT any lemmas on g).

  // Expression *test_expr = rw_pf->equality_proof;
  printf("Context:%s\n", stringify_context2(g_f_a_ctx));
  RewriteProof *rw_pf = rewrite_example(f_f_a);
  printf("Proof term (with let): %s\n", stringify_expression(rw_pf->equality_proof));

  return 0;
}
