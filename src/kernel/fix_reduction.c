#include "src/kernel/fix_reduction.h"

#include <stdlib.h>

#include "src/kernel/subst.h"

bool is_fix_reducible(Expression *expression) { return expression->tag == FIX_EXPRESSION; }

Expression *fix_reduce(Expression *expression) {
    if (!is_fix_reducible(expression)) {
        return NULL;
    }

    Expression *recursive_var = get_fix_recursive_var(expression);
    Expression **args = get_fix_args(expression);
    int arg_count = get_fix_arg_count(expression);
    Expression *body = get_fix_body(expression);
    Context *context = get_expression_context(body);

    Expression *result = new_subst(context, body, recursive_var, expression);
    Context *result_context = get_expression_context(result);

    for (int i = arg_count - 1; i >= 0; i--) {
        // We cannot reuse the same variables due to the parallel substitution.
        Expression *new_var = init_var_expression_wc(get_var_name(args[i]),
                                                     get_expression_type(args[i]), result_context);
        result = init_lambda_expression_wc(new_var, result);
        if (!result) {
            return NULL;
        }
        result_context = new_var;
    }

    return result;
}