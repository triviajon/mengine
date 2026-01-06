#ifndef NORMALIZE_H
#define NORMALIZE_H

#include "src/kernel/expression.h"

// Reduction flags control which reduction rules are applied during normalization
typedef enum {
    REDUCE_BETA = 1 << 0,   // Beta reduction: (fun x => t) u ~> t[x := u]
    REDUCE_DELTA = 1 << 1,  // Delta reduction: unfold definitions
    REDUCE_IOTA = 1 << 2,   // Iota reduction: match on constructors
    REDUCE_FIX = 1 << 3,    // Fix reduction: fixpoint to lambdas
    REDUCE_ALL = REDUCE_BETA | REDUCE_DELTA | REDUCE_IOTA | REDUCE_FIX
} ReductionFlags;

// normalize_cbv reduces an expression using the specified reduction flags.
// The reduction strategy is call-by-value (evaluate arguments before applying).
// Returns a new expression in normal form (as determined by the flags),
// or NULL if normalization fails.
Expression *normalize_cbv(Expression *expr, ReductionFlags flags);

// normalize_compute performs full normalization (all reductions enabled).
// Equivalent to normalize_cbv(expr, REDUCE_ALL).
Expression *normalize_compute(Expression *expr);

// normalize_whnf reduces to weak head normal form (weak-head normalization).
// Only reduces the head of the expression, not inside abstractions.
Expression *normalize_whnf(Expression *expr);

#endif  // NORMALIZE_H
