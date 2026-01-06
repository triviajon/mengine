#include "src/kernel/normalize.h"

#include <stdbool.h>

#include "src/kernel/beta_reduction.h"
#include "src/kernel/delta_reduction.h"
#include "src/kernel/fix_reduction.h"
#include "src/kernel/iota_reduction.h"

static Expression *_normalize_cbv(Expression *expr, ReductionFlags flags) {
    if (!expr) {
        return NULL;
    }

    Expression *current = expr;
    bool changed = true;

    while (changed) {
        changed = false;
        Expression *next = NULL;

        switch (current->tag) {
            case APP_EXPRESSION: {
                Context *ctx = get_expression_context(current);
                Expression *func = get_app_func(current);
                Expression *arg = get_app_arg(current);

                // Normalize subexpressions first (call-by-value)
                Expression *norm_func = _normalize_cbv(func, flags);
                Expression *norm_arg = _normalize_cbv(arg, flags);

                // Try beta reduction if enabled
                if ((flags & REDUCE_BETA) && forms_beta_redex(norm_func, norm_arg)) {
                    next = beta_reduce(ctx, norm_func, norm_arg);
                    if (next) {
                        current = next;
                        changed = true;
                        continue;
                    }
                }

                // Rebuild application if subexpressions changed
                if (norm_func != func || norm_arg != arg) {
                    next = init_app_expression_wc(norm_func, norm_arg, ctx);
                    if (next) {
                        current = next;
                        changed = true;
                        continue;
                    }
                }
                break;
            }

            case VAR_EXPRESSION: {
                // Try delta reduction if enabled
                if ((flags & REDUCE_DELTA) && is_delta_reducible(current)) {
                    next = delta_reduce(current);
                    if (next) {
                        // Continue normalizing the unfolded definition
                        current = next;
                        changed = true;
                        continue;
                    }
                }
                // Variables that can't be delta-reduced are in normal form
                return current;
            }

            case FIX_EXPRESSION: {
                // Try fix reduction if enabled
                if ((flags & REDUCE_FIX) && is_fix_reducible(current)) {
                    next = fix_reduce(current);
                    if (next) {
                        // Continue normalizing the result of fix reduction
                        current = next;
                        changed = true;
                        continue;
                    }
                }
                // FIX expressions that can't be reduced are in normal form
                return current;
            }

            case MATCH_EXPRESSION: {
                Context *ctx = get_expression_context(current);

                // Normalize scrutinee first
                Expression *scrutinee = current->as.match.scrutinee;
                Expression *norm_scrutinee = _normalize_cbv(scrutinee, flags);

                // Rebuild match if scrutinee changed
                Expression *match_expr = current;
                if (norm_scrutinee != scrutinee) {
                    match_expr =
                        init_match_expression_wc(norm_scrutinee, current->as.match.branches,
                                                 current->as.match.branch_count, ctx);
                    if (!match_expr) {
                        break;
                    }
                }

                // Try iota reduction if enabled
                if ((flags & REDUCE_IOTA) && is_iota_reducible(match_expr)) {
                    next = iota_reduce(ctx, match_expr);
                    if (next) {
                        current = next;
                        changed = true;
                        continue;
                    }
                }

                // If scrutinee changed but no iota reduction, update current
                if (match_expr != current) {
                    current = match_expr;
                    changed = true;
                    continue;
                }
                break;
            }

            case LAMBDA_EXPRESSION: {
                // Normalize body
                Expression *body = get_lambda_body(current);
                Expression *norm_body = _normalize_cbv(body, flags);

                if (norm_body != body) {
                    Expression *bv = get_lambda_bound_variable(current);
                    next = init_lambda_expression_wc(bv, norm_body);
                    if (next) {
                        current = next;
                        changed = true;
                        continue;
                    }
                }
                break;
            }

            case FORALL_EXPRESSION: {
                // Normalize body
                Expression *body = get_forall_body(current);
                Expression *norm_body = _normalize_cbv(body, flags);

                if (norm_body != body) {
                    Expression *bv = get_forall_bound_variable(current);
                    next = init_forall_expression_wc(bv, norm_body);
                    if (next) {
                        current = next;
                        changed = true;
                        continue;
                    }
                }
                break;
            }

            case TYPE_EXPRESSION:
            case PROP_EXPRESSION:
            case HOLE_EXPRESSION:
                return current;

            default:
                return current;
        }
    }

    return current;
}

Expression *normalize_cbv(Expression *expr, ReductionFlags flags) {
    return _normalize_cbv(expr, flags);
}

Expression *normalize_compute(Expression *expr) { return _normalize_cbv(expr, REDUCE_ALL); }

Expression *normalize_whnf(Expression *expr) {
    if (!expr) {
        return NULL;
    }

    switch (expr->tag) {
        case APP_EXPRESSION: {
            Context *ctx = get_expression_context(expr);
            Expression *func = get_app_func(expr);
            Expression *arg = get_app_arg(expr);
            if (func->tag == LAMBDA_EXPRESSION) {
                return normalize_whnf(beta_reduce(ctx, func, arg));
            }

            // TODO: Figure out if whnf also consider FIX_EXPRESSIONs. If so, we'd also need to
            // handle FIX_EXPRESSION in the main switch statement to apply fix reduction.
            return expr;
        }

        default:
            return expr;
    }
}
