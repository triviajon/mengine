#include "alpha_equiv.h"
#include "expression.h"
#include "substitute.h"

Expression *safe_synthesize_lambda_type(Expression *gamma, Expression *var,
                                        Expression *type, Expression *body);

Expression *safe_synthesize_app_type(Expression *gamma, Expression *func,
                                     Expression *arg);
