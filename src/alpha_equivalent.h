#ifndef ENV_H
#define ENV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Forward declaration of Expression
typedef struct Expression Expression;

bool alpha_equivalent(Expression *t1, Expression *t2);

#endif // ENV_H