#ifndef UNIFY_H
#define UNIFY_H

#include "kernel/expression.h"

// Create a mapping of variables in exprA to terms in exprB
Context *unify(Expression *exprA, Expression *exprB);

#endif  // UNIFY_H