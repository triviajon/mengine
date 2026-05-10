#ifndef CONVERSION_H
#define CONVERSION_H

#include <stdbool.h>

#include "src/kernel/expression.h"

typedef struct Conversion Conversion;

typedef enum {
    CONVERSION_REFL,
    CONVERSION_SYM,
    CONVERSION_TRANS,
    CONVERSION_NORMALIZE,
    CONVERSION_BETA,
    CONVERSION_DELTA,
    CONVERSION_IOTA,
    CONVERSION_APP,
    CONVERSION_FORALL,
    CONVERSION_LAMBDA,
    CONVERSION_MATCH,
    CONVERSION_FIX,
} ConversionRule;

void conversion_cache_clear(void);

Context *conversion_min_context(Expression *lhs, Expression *rhs);

Conversion *conversion_refl(Context *context, Expression *expr);
Conversion *conversion_sym(Conversion *conv);
Conversion *conversion_trans(Conversion *left, Conversion *right);

Conversion *conversion_check_in_context(Context *context, Expression *lhs, Expression *rhs);
bool conversion_holds_in_context(Context *context, Expression *lhs, Expression *rhs);

ConversionRule conversion_rule(Conversion *conv);
Context *conversion_context(Conversion *conv);
Expression *conversion_lhs(Conversion *conv);
Expression *conversion_rhs(Conversion *conv);
bool conversion_valid_in_context(Conversion *conv, Context *context);

void conversion_free(Conversion *conv);

#endif  // CONVERSION_H
