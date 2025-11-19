#include "rewrite_single_argument.h"

RewriteProof *rewrite_gfa(int f_length, int g_wrap) {
    Expression *current_expr = a;
    for (int i = 0; i < f_length; i++) {
        current_expr = init_app_expression(f, current_expr);
    }

    if (g_wrap) {
        current_expr = init_app_expression(g, current_expr);
    }

    return rewrite(get_expression_context(current_expr), current_expr, eq_fa_a);
}

int main() {
    int f_length = 5;
    int g_wrap = 1;
    RewriteProof *proof = rewrite_gfa(f_length, g_wrap);
    free_rewrite_proof(proof);
    return 0;
}