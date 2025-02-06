#include "main_utils.h"

void print_rwpf__coq_ready(RewriteProof *rw_pf) {
  Expression *original = rw_pf->expr;
  Expression *rewritten = rw_pf->rewritten_expr;
  Expression *proof = rw_pf->equality_proof;

  Context *expr_ctx = get_expression_context(original);
  Expression *expr_ty = get_expression_type(original);

  fprintf(stdout, "Section Test.\n");
  fprintf(stdout, "Require Import Setoid Morphisms.\n");
  fprintf(stdout, "%s\n", stringify_context2(expr_ctx));
  // fprintf(stdout,
  //         "Declare Instance Equivalence_eq : Equivalence eq.\nInstance "
  //         "f_Proper : Proper (eq ==> eq) f := f_equal f.\nInstance f_Proper : "
  //         "Proper (eq ==> eq) g := f_equal g.");
  fprintf(stdout, "\nCheck %s : eq (%s) (%s) (%s).\n",
          stringify_expression2(proof),
          stringify_expression2(expr_ty),
          stringify_expression2(original),
          stringify_expression2(rewritten));
  fprintf(stdout, "End Test.\n");
}

void print_rwpf__no_proof(RewriteProof *rw_pf) {
  Expression *original = rw_pf->expr;
  Expression *rewritten = rw_pf->rewritten_expr;

  Context *expr_ctx = get_expression_context(original);

  fprintf(stdout, "%s\n", stringify_context2(expr_ctx));
  fprintf(stdout, "%s\n -->\n (%s)\n",
          stringify_expression2(original),
          stringify_expression2(rewritten));
  fprintf(stdout, "End Test.\n");
}