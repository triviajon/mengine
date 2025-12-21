#include "beta_reduction.h"

#include "src/kernel/context.h"
#include "src/kernel/subst.h"

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
    return subst(body, old, new);
}

Expression *eval_fix(Expression *app_func, Expression *app_arg) {
    if (app_func->type != FIX_EXPRESSION) {
        return NULL;
    }

    Expression *match_statement = app_func->value.fix.body;
    if (match_statement->type != MATCH_EXPR_EXPRESSION) {
        return NULL;
    }

    Expression *innermost = get_innermost_func(app_arg);
    Expression *match_result = NULL;
    if (innermost == get_innermost_func(get_innermost_body(
                         match_statement->value.matchExpr.literal_case_item))) {
        match_result = match_and_subst(
            get_innermost_body(
                match_statement->value.matchExpr.literal_case_item),
            app_arg,
            get_innermost_body(
                match_statement->value.matchExpr.literal_result));
    } else if (innermost ==
               get_innermost_func(get_innermost_body(
                   match_statement->value.matchExpr.var_case_item))) {
        match_result = match_and_subst(
            get_innermost_body(match_statement->value.matchExpr.var_case_item),
            app_arg,
            get_innermost_body(match_statement->value.matchExpr.var_result));
    } else if (innermost ==
               get_innermost_func(get_innermost_body(
                   match_statement->value.matchExpr.op_case_item))) {
        match_result = match_and_subst(
            get_innermost_body(match_statement->value.matchExpr.op_case_item),
            app_arg,
            get_innermost_body(match_statement->value.matchExpr.op_result));
    }

    if (match_result == NULL) {
        return NULL;
    }

    Expression *ident = app_func->value.fix.ident;
    Expression *replaced_references = subst(match_result, ident, app_func);
    Expression *normalized = normalize(replaced_references);
    return normalized;
}

Expression *normalize(Expression *expression) {
    switch (expression->type) {
        case (APP_EXPRESSION): {
            Expression *new_func = normalize(expression->value.app.func);
            Expression *new_arg = normalize(expression->value.app.arg);
            if (new_func->type == LAMBDA_EXPRESSION) {
                return reduce(new_func, new_arg);
            }
            if (new_func->type == FIX_EXPRESSION) {
                return eval_fix(new_func, new_arg);
            }
            Expression *result = init_app_expression(new_func, new_arg);
            if (!result) {
                return NULL;
            }
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            Expression *new_body = normalize(expression->value.lambda.body);
            return init_lambda_expression(
                expression->value.lambda.bound_variable, new_body);
        }
        case (FORALL_EXPRESSION): {
            Expression *new_body = normalize(expression->value.forall.body);
            return init_forall_expression(
                expression->value.forall.bound_variable, new_body);
        }
        default:
            return expression;
    }
}

void normalize_hole_type(Expression *expression) {
    if (expression->type != HOLE_EXPRESSION) {
        return;
    }

    Expression *expr_type = get_expression_type(expression);
    Expression *normalized_type = normalize(expr_type);
    expression->value.hole.return_type = normalized_type;
}

Expression *weak_head_normalize(Expression *expression) {
    switch (expression->type) {
        case (APP_EXPRESSION): {
            Expression *new_func =
                weak_head_normalize(expression->value.app.func);
            if (new_func->type == LAMBDA_EXPRESSION) {
                return reduce(new_func, expression->value.app.arg);
            }
            if (new_func->type == FIX_EXPRESSION) {
                return eval_fix(new_func, expression->value.app.arg);
            }
            Expression *result =
                init_app_expression(new_func, expression->value.app.arg);
            if (!result) {
                return NULL;
            }
            return result;
        }
        default:
            return expression;
    }
}