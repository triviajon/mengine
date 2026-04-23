#include "beta_reduction.h"

#include "src/kernel/subst.h"

bool forms_beta_redex(Expression *app_func, Expression *app_arg) {
    return (app_func != NULL && (app_func->tag == LAMBDA_EXPRESSION) && app_arg != NULL) != 0;
}

Expression *beta_reduce(Context *gamma, Expression *app_func, Expression *app_arg) {
    if (app_func->tag != LAMBDA_EXPRESSION) {
        return NULL;
    }

    Expression *body = get_lambda_body(app_func);
    Expression *old = get_lambda_bound_variable(app_func);
    Expression *new = app_arg;
    return beta_subst(gamma, body, old, new);
}

Expression *weak_head_normalize(Expression *expression) {
    switch (expression->tag) {
        case (APP_EXPRESSION): {
            Context *app_context = get_expression_context(expression);
            Expression *new_func = weak_head_normalize(get_app_func(expression));
            if (new_func->tag == LAMBDA_EXPRESSION) {
                return beta_reduce(app_context, new_func, get_app_arg(expression));
            }
            Expression *result =
                init_app_expression_wc(new_func, get_app_arg(expression), app_context);
            if (!result) {
                return NULL;
            }
            return result;
        }
        default:
            return expression;
    }
}