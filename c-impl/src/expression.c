#include "expression.h"

Expression *init_type_expression() {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    if (!expr) return NULL;
    expr->type = TYPE_EXPRESSION;
    expr->value.type.uplinks = create_doubly_linked_list();
    return expr;
}

void free_type_expression(Expression *expr) {
    if (!expr) return;
    free_doubly_linked_list(expr->value.type.uplinks);
    free(expr);
}
