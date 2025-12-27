#include "beta_reduction.h"

#include "src/kernel/context.h"
#include "src/kernel/new_subst.h"

bool forms_redex(Expression *app_func, Expression *app_arg) {
    return app_func != NULL && (app_func->tag == LAMBDA_EXPRESSION) && app_arg != NULL;
}

bool is_redex(Expression *app_expr) {
    if (app_expr->tag != APP_EXPRESSION) {
        return false;
    }

    Expression *app_func = get_app_func(app_expr);
    return (app_func->tag == LAMBDA_EXPRESSION);
}

Expression *reduce(Expression *app_func, Expression *app_arg) {
    if (app_func->tag != LAMBDA_EXPRESSION) {
        // printf("Expression is not a redex.");
        return NULL;
    }

    Expression *body = get_lambda_body(app_func);
    Expression *old = get_lambda_bound_variable(app_func);
    Expression *new = app_arg;
    // body is closed under context(old), which contains old
    return new_subst(old, body, old, new);
}

Expression *weak_head_normalize(Expression *expression) {
    switch (expression->tag) {
        case (APP_EXPRESSION): {
            Expression *new_func = weak_head_normalize(get_app_func(expression));
            if (new_func->tag == LAMBDA_EXPRESSION) {
                return reduce(new_func, get_app_arg(expression));
            }
            Context *app_context = get_expression_context(expression);
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