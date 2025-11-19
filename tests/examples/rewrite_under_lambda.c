#include "rewrite_under_lambda.h"

RewriteProof *rewrite_lambda_f_x() {
    Expression *x = init_var_expression("x", nat);
    Expression *f = init_var_expression("f", init_arrow_expression(nat, nat));
    Expression *expr = init_lambda_expression(
        f, init_lambda_expression(x, init_app_expression(f, x)));

    return rewrite(get_expression_context(expr), expr, eq_fa_a);
}

int main() {
    RewriteProof *proof = rewrite_lambda_f_x();
    free_rewrite_proof(proof);
    return 0;
}