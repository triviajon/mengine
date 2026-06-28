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

                // Rebuild the application with normalized subexpressions (the args
                // are now values, since cbv normalizes them first).
                Expression *rebuilt = current;
                if (norm_func != func || norm_arg != arg) {
                    rebuilt = init_app_expression_wc(norm_func, norm_arg, ctx);
                    if (!rebuilt) {
                        break;
                    }
                }

                // Guarded fix reduction on an applied fixpoint (mirrors
                // normalize_whnf): unfold `(fix ...) args` one step only when the
                // decreasing argument is constructor-headed, so the surrounding match
                // can then iota-reduce. The guard prevents runaway unfolding on a
                // symbolic recursive argument.
                if (flags & REDUCE_FIX) {
                    Expression *unfolded = fix_reduce_app(rebuilt, normalize_whnf);
                    if (unfolded) {
                        current = unfolded;
                        changed = true;
                        continue;
                    }
                }

                if (rebuilt != current) {
                    current = rebuilt;
                    changed = true;
                    continue;
                }
                break;
            }

            case VAR_EXPRESSION: {
                // Try delta reduction if enabled
                if ((flags & REDUCE_DELTA) && is_delta_reducible(current)) {
                    next = delta_reduce(current);
                    if (next && next->tag == FIX_EXPRESSION) {
                        // A constant whose definition is a fixpoint is a value here; it
                        // fires only when applied to a constructor-headed decreasing
                        // argument (handled in the APP case via fix_reduce_app). Holding
                        // it folded keeps a stuck residual constant-headed (`add n O`,
                        // not `(fix ...) n O`) so rewrite/congruence can match it.
                        return current;
                    }
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
                // A bare fix is a value; it reduces only once applied to a
                // constructor-headed decreasing argument (see the APP case).
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
                return current;
            }

            case FORALL_EXPRESSION: {
                return current;
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

    Expression *current = expr;

    while (true) {
        switch (current->tag) {
            case VAR_EXPRESSION: {
                if (is_delta_reducible(current)) {
                    Expression *next = delta_reduce(current);
                    if (next && next->tag == FIX_EXPRESSION) {
                        // Fixpoint constant: a value here, fires in the APP case via
                        // fix_reduce_app. Holding it folded keeps stuck residuals
                        // constant-headed and matchable (see _normalize_cbv).
                        return current;
                    }
                    if (next) {
                        current = next;
                        continue;
                    }
                }
                return current;
            }

            case APP_EXPRESSION: {
                Context *ctx = get_expression_context(current);
                Expression *func = get_app_func(current);
                Expression *arg = get_app_arg(current);

                // First normalize the function to WHNF
                Expression *norm_func = normalize_whnf(func);

                if (norm_func != func) {
                    current = init_app_expression_wc(norm_func, arg, ctx);
                    continue;
                }

                if (norm_func->tag == LAMBDA_EXPRESSION) {
                    current = beta_reduce(ctx, norm_func, arg);
                    continue;
                }

                // Guarded fix reduction: only fire when the decreasing argument is
                // constructor-headed (see fix_reduce_app), preventing runaway
                // unfolding on symbolic recursive arguments.
                Expression *fix_unfolded = fix_reduce_app(current, normalize_whnf);
                if (fix_unfolded) {
                    current = fix_unfolded;
                    continue;
                }

                return current;
            }

            case FIX_EXPRESSION: {
                // A bare fix is a value; reduction happens in APP_EXPRESSION once a
                // constructor-headed decreasing argument is applied.
                return current;
            }

            case MATCH_EXPRESSION: {
                Context *ctx = get_expression_context(current);

                Expression *scrut = current->as.match.scrutinee;
                Expression *norm_scrut = normalize_whnf(scrut);

                if (norm_scrut != scrut) {
                    current = init_match_expression_wc(norm_scrut, current->as.match.branches,
                                                       current->as.match.branch_count, ctx);
                    continue;
                }

                if (is_iota_reducible(current)) {
                    Expression *next = iota_reduce(ctx, current);
                    if (next) {
                        current = next;
                        continue;
                    }
                }

                return current;
            }

            case LAMBDA_EXPRESSION:
            case FORALL_EXPRESSION:
            case TYPE_EXPRESSION:
            case PROP_EXPRESSION:
            case HOLE_EXPRESSION:
                return current;

            default:
                return current;
        }
    }
}
