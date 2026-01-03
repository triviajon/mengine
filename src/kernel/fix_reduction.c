#include "src/kernel/fix_reduction.h"

#include <stdio.h>
#include <stdlib.h>

#include "new_subst.h"

bool is_fix_reducible(Expression *expression) { return expression->tag == FIX_EXPRESSION; }

Expression *fix_reduce(Expression *expression) {
    if (!is_fix_reducible(expression)) {
        return NULL;
    }

    Expression *recursive_var = get_fix_recursive_var(expression);
    Expression **args = get_fix_args(expression);
    int arg_count = get_fix_arg_count(expression);
    Expression *body = get_fix_body(expression);

    Context *body_context = args[arg_count - 1];

    Expression *result = new_subst(body_context, body, recursive_var, expression);
    if (!result) {
        return NULL;
    }

    for (int i = arg_count - 1; i >= 0; i--) {
        result = init_lambda_expression_wc(args[i], result);
        if (!result) {
            return NULL;
        }
    }

    return result;
}
