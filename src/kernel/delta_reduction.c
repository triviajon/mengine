#include "src/kernel/expression.h"

bool is_delta_reducible(Expression *expression) {
    return expression->tag == VAR_EXPRESSION && expression->as.var.body != NULL;
}

Expression *delta_reduce(Expression *expression) {
    if (!is_delta_reducible(expression)) {
        return NULL;
    }

    return get_var_body(expression);
}