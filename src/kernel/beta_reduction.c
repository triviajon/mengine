#include "beta_reduction.h"

#include "src/kernel/context.h"
#include "src/kernel/new_subst.h"

bool forms_redex(Expression *app_func, Expression *app_arg) {
    return app_func != NULL && (app_func->type == LAMBDA_EXPRESSION) &&
           app_arg != NULL;
}

bool is_redex(Expression *app_expr) {
    if (app_expr->type != APP_EXPRESSION) {
        return false;
    }

    Expression *app_func = app_expr->value.app.func;
    return (app_func->type == LAMBDA_EXPRESSION);
}

Expression *reduce(Expression *app_func, Expression *app_arg) {
    if (app_func->type != LAMBDA_EXPRESSION) {
        // printf("Expression is not a redex.");
        return NULL;
    }

    Expression *body = app_func->value.lambda.body;
    Expression *old = app_func->value.lambda.bound_variable;
    Expression *new = app_arg;
    return new_subst(body, old, new);
}

Expression *weak_head_normalize(Expression *expression) {
    switch (expression->type) {
        case (APP_EXPRESSION): {
            Expression *new_func =
                weak_head_normalize(expression->value.app.func);
            if (new_func->type == LAMBDA_EXPRESSION) {
                return reduce(new_func, expression->value.app.arg);
            }
            Context *app_context = get_expression_context(expression);
            Expression *result = init_app_expression_wc(
                new_func, expression->value.app.arg, app_context);
            if (!result) {
                return NULL;
            }
            return result;
        }
        default:
            return expression;
    }
}