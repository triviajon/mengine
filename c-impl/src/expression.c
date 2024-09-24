#include "expression.h"

Theorem *init_theorem(char *name, Expression *theorem, Expression *proof) {
    Theorem *new_theorem = (Theorem *)malloc(sizeof(Theorem));
    if (new_theorem == NULL) {
        return NULL;
    }

    new_theorem->name = (char *)malloc(strlen(name) + 1);
    if (new_theorem->name == NULL) {
        free(new_theorem);
        return NULL;
    }
    strcpy(new_theorem->name, name);

    new_theorem->theorem = theorem;
    new_theorem->proof = proof;

    return new_theorem;
}

Step *init_let_step(char *id, Expression *expr, Step *next) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = LET_STEP;
    step->value.let.id = id;
    step->value.let.expr = expr;
    step->next = next;
    return step;
}

Step *init_theorem_step(Theorem *theorem, Step *next) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = THEOREM_STEP;
    step->value.theorem.theorem = theorem;
    step->next = next;
    return step;
}

Step *init_expr_step(Expression *expr) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = EXPR_STEP;
    step->value.expr.expr = expr;
    step->next = NULL;
    return step;
}

Step *init_end_step() {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = END_STEP;
    step->next = NULL;
    return step;
}

// Expression initialization functions
Expression *init_var_expression(const char *name) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.uplinks = dll_create();
    return expr;
}

Expression *init_lambda_expression(Expression *var, Expression *type, Expression *body) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = LAMBDA_EXPRESSION;
    expr->value.lambda.var = var;
    expr->value.lambda.type = type;
    expr->value.lambda.body = body;
    expr->value.lambda.uplinks = dll_create();
    return expr;
}

Expression *init_app_expression(Expression *func, Expression *arg) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = APP_EXPRESSION;
    expr->value.app.func = func;
    expr->value.app.arg = arg;
    expr->value.app.cache = NULL;
    expr->value.app.uplinks = dll_create();
    return expr;
}

Expression *init_forall_expression(Expression *var, Expression *type, Expression *arg) {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = FORALL_EXPRESSION;
    expr->value.forall.var = var;
    expr->value.forall.type = type;
    expr->value.forall.arg = arg;
    expr->value.forall.uplinks = dll_create();
    return expr;
}

Expression *init_type_expression() {
    Expression *expr = (Expression*)malloc(sizeof(Expression));
    expr->type = TYPE_EXPRESSION;
    expr->value.type.uplinks = dll_create();
    return expr;
}

Expression *free_var_expression(Expression *expr) {
    if (expr && expr->type == VAR_EXPRESSION) {
        free(expr->value.var.name);
        dll_destroy(expr->value.var.uplinks);
        free(expr);
    }
    return NULL;
}

Expression *free_lambda_expression(Expression *expr) {
    if (expr && expr->type == LAMBDA_EXPRESSION) {
        free_lambda_expression(expr->value.lambda.var);
        free_lambda_expression(expr->value.lambda.body);
        free_lambda_expression(expr->value.lambda.type);
        dll_destroy(expr->value.lambda.uplinks);
        free(expr);
    }
    return NULL;
}

Expression *free_app_expression(Expression *expr) {
    if (expr && expr->type == APP_EXPRESSION) {
        free_app_expression(expr->value.app.func);
        free_app_expression(expr->value.app.arg);
        if (expr->value.app.cache) {
            free_app_expression(expr->value.app.cache);
        }
        dll_destroy(expr->value.app.uplinks);
        free(expr);
    }
    return NULL;
}

Expression *free_forall_expression(Expression *expr) {
    if (expr && expr->type == FORALL_EXPRESSION) {
        free_forall_expression(expr->value.forall.var);
        free_forall_expression(expr->value.forall.type);
        free_forall_expression(expr->value.forall.arg);
        dll_destroy(expr->value.forall.uplinks);
        free(expr);
    }
    return NULL;
}

Expression *free_type_expression(Expression *expr) {
    if (expr && expr->type == TYPE_EXPRESSION) {
        dll_destroy(expr->value.type.uplinks);
        free(expr);
    }
    return NULL;
}

bool equal(Expression *a, Expression *b) {
  return a == b;
}

// Helper function to concatenate two strings
char *str_concat(const char *s1, const char *s2) {
    char *result = (char *)malloc(strlen(s1) + strlen(s2) + 1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

// Helper function to add parentheses around an expression if needed
char *parenthesize(char *expr_str) {
    char *result = (char *)malloc(strlen(expr_str) + 3);
    strcpy(result, "(");
    strcat(result, expr_str);
    strcat(result, ")");
    free(expr_str);
    return result;
}

char *stringify_expression(Expression *expression) {
    char *result = NULL;

    switch (expression->type) {
        case VAR_EXPRESSION:
            result = strdup(expression->value.var.name);
            break;

        case LAMBDA_EXPRESSION: {
            char *var_str = stringify_expression((Expression *)expression->value.lambda.var);
            char *type_str = stringify_expression(expression->value.lambda.type);
            char *body_str = stringify_expression(expression->value.lambda.body);
            result = str_concat("λ", var_str);
            result = str_concat(result, ":");
            result = str_concat(result, type_str);
            result = str_concat(result, " → ");
            result = str_concat(result, body_str);
            free(var_str);
            free(type_str);
            free(body_str);
            break;
        }

        case APP_EXPRESSION: {
            char *func_str = stringify_expression(expression->value.app.func);
            char *arg_str = stringify_expression(expression->value.app.arg);
            result = str_concat(parenthesize(func_str), " ");
            result = str_concat(result, parenthesize(arg_str));
            free(func_str);
            free(arg_str);
            break;
        }

        case FORALL_EXPRESSION: {
            char *var_str = stringify_expression((Expression *)expression->value.forall.var);
            char *type_str = stringify_expression(expression->value.forall.type);
            char *arg_str = stringify_expression(expression->value.forall.arg);
            result = str_concat("∀", var_str);
            result = str_concat(result, ":");
            result = str_concat(result, type_str);
            result = str_concat(result, "; ");
            result = str_concat(result, arg_str);
            free(var_str);
            free(type_str);
            free(arg_str);
            break;
        }

        case TYPE_EXPRESSION:
            result = strdup("Type");
            break;

        default:
            result = strdup("Unknown expression");
            break;
    }

    return result;
}

Expression *set_in_context(Expression *context, Expression *var, Expression *expr) {
    switch (context->type) {
    case (TYPE_EXPRESSION):
        return init_forall_expression(var, expr, context);
    case (FORALL_EXPRESSION): 
        return init_forall_expression(context->value.forall.var, context->value.forall.type, set_in_context(context->value.forall.arg, var, expr));
    default:
        // panic!
        return NULL;
    }
}

Expression *lookup_in_context(Expression *context, Expression *var) {
    Expression *result = NULL; 
    Expression *current_context = context;
    while (current_context->type == FORALL_EXPRESSION) {
        Expression *forall_var = current_context->value.forall.var;
        if (equal(forall_var, var)) {
            result = current_context->value.forall.type;
        }
        current_context = current_context->value.forall.arg;
    }
    return result;
}

Expression *top_context(Expression *context) {
    switch (context->type) {
    case (FORALL_EXPRESSION): 
        switch (context->value.forall.arg->type) {
        case (TYPE_EXPRESSION):
            return context;
        case (FORALL_EXPRESSION): 
            return top_context(context->value.forall.arg);        
        default:
            // panic!
            return NULL;    
        }
    default:
        // panic!
        return NULL;
    }
}

Expression *clear_top_context(Expression *context) {
    switch (context->type) {
    case (FORALL_EXPRESSION): 
        switch (context->value.forall.arg->type) {
        case (TYPE_EXPRESSION):
            return init_type_expression();
        case (FORALL_EXPRESSION): 
            return init_forall_expression(context->value.forall.var, context->value.forall.type, clear_top_context(context->value.forall.arg));
        default:
            // panic!
            return NULL;    
        }
    default:
        // panic!
        return NULL;
    }
}