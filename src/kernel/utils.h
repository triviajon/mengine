#include "expression.h"

typedef struct Context Context;

char *stringify_expression(Expression *expression);
char *stringify_context(Context *context);

// Two expressions are equal wrt this function if their pointers are equal
bool expression_equal(Expression *a, Expression *b);
