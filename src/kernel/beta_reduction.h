#include "expression.h"

Expression *reduce_body(Expression *body, Expression *old, Expression *new);
Expression *reduce(Expression *app_func, Expression *app_arg);