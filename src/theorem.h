#include "expression.h"

typedef struct {
  char *name;
  Expression *theorem;
  Expression *proof;
} Theorem;

Theorem *init_theorem(char *name, Expression *theorem, Expression *proof);
