#include "main_utils.h"

void print_rwpf__coq_ready(RewriteProof *rw_pf, int withlet_flag,
                           int debug_flag) {
    Expression *original = rw_pf->expr;
    Expression *rewritten = rw_pf->rewritten_expr;
    Expression *proof = rw_pf->equality_proof;

    Context *proof_ctx = get_expression_context(proof);
    Expression *expr_ty = get_expression_type(original);

    fprintf(stdout, "Section Test.\n");
    fprintf(stdout, "Require Import Setoid Morphisms.\n");
    fprintf(stdout, "%s\n", stringify_context2(proof_ctx));
    if (withlet_flag > 0) {
        fprintf(stdout, "\nCheck %s : eq (%s) (%s) (%s).\n",
                stringify_expression_with_let2(proof),
                stringify_expression2(expr_ty),
                stringify_expression_with_let2(original),
                stringify_expression_with_let2(rewritten));
    } else {
        fprintf(stdout, "\nCheck %s : eq (%s) (%s) (%s).\n",
                stringify_expression2(proof), stringify_expression2(expr_ty),
                stringify_expression2(original),
                stringify_expression2(rewritten));
    }

    fprintf(stdout, "End Test.\n");
    if (debug_flag) {
    }
}

void print_rwpf__no_proof(RewriteProof *rw_pf, int withlet_flag,
                          int debug_flag) {
    Expression *original = rw_pf->expr;
    Expression *rewritten = rw_pf->rewritten_expr;

    Context *expr_ctx = get_expression_context(original);

    fprintf(stdout, "%s\n", stringify_context2(expr_ctx));
    if (withlet_flag > 0) {
        fprintf(stdout, "%s\n", stringify_expression_with_let2(rewritten));
    } else {
        fprintf(stdout, "%s\n", stringify_expression2(rewritten));
    }

    if (debug_flag) {
    }
}