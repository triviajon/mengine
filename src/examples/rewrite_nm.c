#include "rewrite_nm.h"

Expression *f_nm = NULL;
Expression *x0_nm = NULL;
Expression *f_n_x0 = NULL;

RewriteProof *rewrite_nm(int n, int m) {
    if (n < 1 || m < 1) {
        fprintf(stderr, "Error: n and m must be at least 1.\n");
        return NULL;
    }

    Expression *f_nm_ty = nat;
    for (int i = 0; i < n; i++) {
        f_nm_ty = init_arrow_expression(nat, f_nm_ty);
    }

    f_nm = init_var_expression("f_nm", f_nm_ty);
    x0_nm = init_var_expression("x0_nm", nat);

    // Lemma f_n_x0 : f x0 x0 ... x0 = x0.
    Expression *lemma_lhs = f_nm;
    for (int i = 0; i < n; i++) {
        lemma_lhs = init_app_expression(lemma_lhs, x0_nm);
    }
    Expression *lemma_rhs = x0_nm;
    Expression *lemma_ty = init_app_expression(init_app_expression(init_app_expression(eq, nat), lemma_lhs), lemma_rhs);
    f_n_x0 = init_var_expression("f_n_x0", lemma_ty);

    Expression *last_expr = x0_nm;  // Start with x0
    
    for (int i = 0; i < m; i++) {
        Expression *f_app = f_nm;
        for (int j = 0; j < n; j++) {
            f_app = init_app_expression(f_app, last_expr);
        }
        last_expr = f_app;
    }
    
    Expression *expr = last_expr;
    
    return rewrite(get_expression_context(expr), expr, f_n_x0);
}