#include "expression.h"

// Expression initialization functions
Expression *init_var_expression(const char *name) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.uplinks = dll_create();
    return expr;
}

Expression *init_lambda_expression(Context *context, Expression *body) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = LAMBDA_EXPRESSION;
    expr->value.lambda.context = context;
    expr->value.lambda.type = NULL; // TODO
    expr->value.lambda.body = body;
    expr->value.lambda.uplinks = dll_create();
    return expr;
}

Expression *init_app_expression(Context *context, Expression *func, Expression *arg) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = APP_EXPRESSION;
    expr->value.app.context = context;
    expr->value.app.func = func;
    expr->value.app.arg = arg;
    expr->value.app.cache = NULL;
    expr->value.app.uplinks = dll_create();
    return expr;
}

Expression *init_forall_expression(Context *context, Expression *body) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = FORALL_EXPRESSION;
    expr->value.forall.context = context;
    expr->value.forall.type = NULL; // TODO
    expr->value.forall.body = body;
    expr->value.forall.uplinks = dll_create();
    return expr;
}

Expression *init_type_expression() {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = TYPE_EXPRESSION;
    expr->value.type.uplinks = dll_create();
    return expr;
}

Expression *init_hole_expression(char *name, Expression *type, Context *context) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = HOLE_EXPRESSION;
    expr->value.hole.name = name;
    expr->value.hole.type = type;
    expr->value.hole.context = context;
    expr->value.forall.uplinks = dll_create();
    return expr;
}

// Forward declarations. No need to expose them in expression.h. 
void free_var_expression(Expression *expr);
void free_lambda_expression(Expression *expr);
void free_app_expression(Expression *expr);
void free_forall_expression(Expression *expr);
void free_type_expression(Expression *expr);
void free_hole_expression(Expression *expr);

void free_expression(Expression *expr) {
    switch (expr->type) {
        case (VAR_EXPRESSION): free_var_expression(expr); break;
        case (LAMBDA_EXPRESSION): free_lambda_expression(expr); break;
        case (APP_EXPRESSION): free_app_expression(expr); break;
        case (FORALL_EXPRESSION): free_forall_expression(expr); break;
        case (TYPE_EXPRESSION): free_type_expression(expr); break;
        case (HOLE_EXPRESSION): free_hole_expression(expr); break;
    }
}

void free_var_expression(Expression *expr) {
    if (expr && expr->type == VAR_EXPRESSION) {
        free(expr->value.var.name);
        dll_destroy(expr->value.var.uplinks);
        free(expr);
    }
}

void free_lambda_expression(Expression *expr) {
    if (expr && expr->type == LAMBDA_EXPRESSION) {
        context_free(expr->value.lambda.context);
        free_expression(expr->value.lambda.type);
        free_expression(expr->value.lambda.body);
        dll_destroy(expr->value.lambda.uplinks);
        free(expr);
    }
}

void free_app_expression(Expression *expr) {
    if (expr && expr->type == APP_EXPRESSION) {
        free_expression(expr->value.app.func);
        free_expression(expr->value.app.arg);
        if (expr->value.app.cache) {
            free_expression(expr->value.app.cache);
        }
        free_expression(expr->value.app.type);
        context_free(expr->value.app.context);
        dll_destroy(expr->value.app.uplinks);
        free(expr);
    }
}

void free_forall_expression(Expression *expr) {
    if (expr && expr->type == FORALL_EXPRESSION) {
        context_free(expr->value.forall.context);
        free_expression(expr->value.forall.type);
        free_expression(expr->value.forall.body);
        dll_destroy(expr->value.forall.uplinks);
        free(expr);
    }
}

void free_type_expression(Expression *expr) {
    if (expr && expr->type == TYPE_EXPRESSION) {
        dll_destroy(expr->value.type.uplinks);
        free(expr);
    }
}

void free_hole_expression(Expression *expr) {
    if (expr && expr->type == HOLE_EXPRESSION) {
        free(expr->value.hole.name);
        free_expression(expr->value.hole.type);
        context_free(expr->value.hole.context);
        dll_destroy(expr->value.forall.uplinks);
        free(expr);
    }
}