#include "src/kernel/conversion.h"

#include <stdint.h>
#include <stdlib.h>

#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/delta_reduction.h"
#include "src/kernel/fix_reduction.h"
#include "src/kernel/iota_reduction.h"

struct Conversion {
    ConversionRule rule;
    Context *context;
    Expression *lhs;
    Expression *rhs;
    Conversion *left;
    Conversion *right;
};

Context *conversion_min_context(Expression *lhs, Expression *rhs) {
    if (!lhs || !rhs) {
        return NULL;
    }

    Context *lhs_ctx = get_expression_context(lhs);
    Context *rhs_ctx = get_expression_context(rhs);

    if (context_is_ancestor(lhs_ctx, rhs_ctx)) {
        return rhs_ctx;
    }
    if (context_is_ancestor(rhs_ctx, lhs_ctx)) {
        return lhs_ctx;
    }
    return NULL;
}

static Conversion *conversion_alloc(ConversionRule rule, Context *context, Expression *lhs,
                                    Expression *rhs) {
    if (!context || !lhs || !rhs) {
        return NULL;
    }
    if (!valid_in_context(lhs, context) || !valid_in_context(rhs, context)) {
        return NULL;
    }

    Conversion *conv = calloc(1, sizeof(Conversion));
    if (!conv) {
        return NULL;
    }
    conv->rule = rule;
    conv->context = context;
    conv->lhs = lhs;
    conv->rhs = rhs;
    return conv;
}

Conversion *conversion_refl(Context *context, Expression *expr) {
    if (!context || !expr || !valid_in_context(expr, context)) {
        return NULL;
    }
    return conversion_alloc(CONVERSION_REFL, get_expression_context(expr), expr, expr);
}

Conversion *conversion_sym(Conversion *conv) {
    if (!conv) {
        return NULL;
    }

    Conversion *result = conversion_alloc(CONVERSION_SYM, conv->context, conv->rhs, conv->lhs);
    if (!result) {
        return NULL;
    }
    result->left = conv;
    return result;
}

Conversion *conversion_trans(Conversion *left, Conversion *right) {
    if (!left || !right || left->rhs != right->lhs) {
        return NULL;
    }

    Context *ctx = left->context;
    if (!context_is_ancestor(right->context, ctx)) {
        if (!context_is_ancestor(ctx, right->context)) {
            return NULL;
        }
        ctx = right->context;
    }

    Conversion *result = conversion_alloc(CONVERSION_TRANS, ctx, left->lhs, right->rhs);
    if (!result) {
        return NULL;
    }
    result->left = left;
    result->right = right;
    return result;
}

#define CONV_TRUE  ((void *)(intptr_t)1)
#define CONV_FALSE ((void *)(intptr_t)2)

typedef struct ConversionStepEntry {
    ConversionRule rule;
    Context *context;
    Expression *from;
    Expression *to;
} ConversionStepEntry;

typedef struct ConversionWhnfEntry {
    Context *context;
    Expression *from;
    Expression *normal;
} ConversionWhnfEntry;

static Map *g_conversion_cache = NULL;
static Map *g_conversion_step_cache = NULL;
static Map *g_conversion_whnf_cache = NULL;

static void conversion_cache_free_inner(void *inner) { map_free((Map *)inner); }

static void conversion_cache_free_value(void *value) { free(value); }

void conversion_cache_clear(void) {
    if (g_conversion_cache != NULL) {
        map_clear_apply_free(g_conversion_cache, conversion_cache_free_inner);
        g_conversion_cache = NULL;
    }
    if (g_conversion_step_cache != NULL) {
        map_clear_apply_free(g_conversion_step_cache, conversion_cache_free_value);
        g_conversion_step_cache = NULL;
    }
    if (g_conversion_whnf_cache != NULL) {
        map_clear_apply_free(g_conversion_whnf_cache, conversion_cache_free_value);
        g_conversion_whnf_cache = NULL;
    }
}

static bool conversion_cacheable_expr(Expression *expr) { return expr && !expr->has_evar; }

static ConversionStepEntry *conversion_cached_step(Expression *expr) {
    if (!conversion_cacheable_expr(expr) || !g_conversion_step_cache) {
        return NULL;
    }
    return (ConversionStepEntry *)map_get(g_conversion_step_cache, expr);
}

static ConversionWhnfEntry *conversion_cached_whnf_entry(Expression *expr) {
    if (!conversion_cacheable_expr(expr) || !g_conversion_whnf_cache) {
        return NULL;
    }
    return (ConversionWhnfEntry *)map_get(g_conversion_whnf_cache, expr);
}

static void conversion_record_step(Context *context, Expression *from, Expression *to,
                                   ConversionRule rule) {
    if (!context || !conversion_cacheable_expr(from) || !to || !valid_in_context(to, context)) {
        return;
    }
    if (!g_conversion_step_cache) {
        g_conversion_step_cache = map_new_with_capacity(64);
        if (!g_conversion_step_cache) {
            return;
        }
    }

    ConversionStepEntry *entry = (ConversionStepEntry *)map_get(g_conversion_step_cache, from);
    if (!entry) {
        entry = malloc(sizeof(ConversionStepEntry));
        if (!entry) {
            return;
        }
        if (!map_set(g_conversion_step_cache, from, entry)) {
            free(entry);
            return;
        }
    }
    entry->rule = rule;
    entry->context = context;
    entry->from = from;
    entry->to = to;
}

static void conversion_record_whnf(Expression *from, Expression *normal) {
    if (!conversion_cacheable_expr(from) || !normal) {
        return;
    }

    Context *context = get_expression_context(from);
    if (!valid_in_context(normal, context)) {
        return;
    }
    if (!g_conversion_whnf_cache) {
        g_conversion_whnf_cache = map_new_with_capacity(64);
        if (!g_conversion_whnf_cache) {
            return;
        }
    }

    ConversionWhnfEntry *entry = (ConversionWhnfEntry *)map_get(g_conversion_whnf_cache, from);
    if (!entry) {
        entry = malloc(sizeof(ConversionWhnfEntry));
        if (!entry) {
            return;
        }
        if (!map_set(g_conversion_whnf_cache, from, entry)) {
            free(entry);
            return;
        }
    }
    entry->context = context;
    entry->from = from;
    entry->normal = normal;
}

static Expression *conversion_whnf(Expression *expr) {
    if (!expr) {
        return NULL;
    }

    ConversionWhnfEntry *cached = conversion_cached_whnf_entry(expr);
    if (cached) {
        return cached->normal;
    }

    Expression *start = expr;
    Expression *current = expr;

    while (true) {
        ConversionWhnfEntry *current_cached = conversion_cached_whnf_entry(current);
        if (current_cached) {
            current = current_cached->normal;
            break;
        }

        ConversionStepEntry *step = conversion_cached_step(current);
        if (step) {
            current = step->to;
            continue;
        }

        switch (current->tag) {
            case VAR_EXPRESSION: {
                if (is_delta_reducible(current)) {
                    Expression *next = delta_reduce(current);
                    if (next) {
                        conversion_record_step(get_expression_context(current), current, next,
                                               CONVERSION_DELTA);
                        current = next;
                        continue;
                    }
                }
                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;
            }

            case APP_EXPRESSION: {
                Context *ctx = get_expression_context(current);
                Expression *func = get_app_func(current);
                Expression *arg = get_app_arg(current);

                Expression *norm_func = conversion_whnf(func);

                if (norm_func != func) {
                    Expression *next = init_app_expression_wc(norm_func, arg, ctx);
                    if (next) {
                        conversion_record_step(ctx, current, next, CONVERSION_APP);
                        current = next;
                        continue;
                    }
                }

                if (norm_func->tag == LAMBDA_EXPRESSION) {
                    Expression *next = beta_reduce(ctx, norm_func, arg);
                    if (next) {
                        conversion_record_step(ctx, current, next, CONVERSION_BETA);
                        current = next;
                        continue;
                    }
                }

                if (norm_func->tag == FIX_EXPRESSION && is_fix_reducible(norm_func)) {
                    Expression *unfolded = fix_reduce(norm_func);
                    if (unfolded) {
                        Expression *next = init_app_expression_wc(unfolded, arg, ctx);
                        if (next) {
                            conversion_record_step(ctx, current, next, CONVERSION_FIX);
                            current = next;
                            continue;
                        }
                    }
                }

                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;
            }

            case FIX_EXPRESSION: {
                if (is_fix_reducible(current)) {
                    Expression *next = fix_reduce(current);
                    if (next) {
                        conversion_record_step(get_expression_context(current), current, next,
                                               CONVERSION_FIX);
                        current = next;
                        continue;
                    }
                }
                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;
            }

            case MATCH_EXPRESSION: {
                Context *ctx = get_expression_context(current);
                Expression *scrut = current->as.match.scrutinee;
                Expression *norm_scrut = conversion_whnf(scrut);

                if (norm_scrut != scrut) {
                    Expression *next = init_match_expression_wc(norm_scrut,
                                                               current->as.match.branches,
                                                               current->as.match.branch_count,
                                                               ctx);
                    if (next) {
                        conversion_record_step(ctx, current, next, CONVERSION_MATCH);
                        current = next;
                        continue;
                    }
                }

                if (is_iota_reducible(current)) {
                    Expression *next = iota_reduce(ctx, current);
                    if (next) {
                        conversion_record_step(ctx, current, next, CONVERSION_IOTA);
                        current = next;
                        continue;
                    }
                }

                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;
            }

            case LAMBDA_EXPRESSION:
            case FORALL_EXPRESSION:
            case TYPE_EXPRESSION:
            case PROP_EXPRESSION:
            case HOLE_EXPRESSION:
                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;

            default:
                conversion_record_whnf(current, current);
                conversion_record_whnf(start, current);
                return current;
        }
    }

    conversion_record_whnf(start, current);
    return current;
}

static bool conversion_derivable(Expression *lhs, Expression *rhs, Map *bv_map) {
    lhs = conversion_whnf(lhs);
    rhs = conversion_whnf(rhs);

    if (!lhs || !rhs) {
        return false;
    }
    if (lhs == rhs) {
        return true;
    }
    if (lhs->tag != rhs->tag) {
        return false;
    }

    switch (lhs->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return true;

        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            return map_get(bv_map, lhs) == rhs;

        case APP_EXPRESSION:
            return conversion_derivable(lhs->as.app.func, rhs->as.app.func, bv_map) &&
                   conversion_derivable(lhs->as.app.arg, rhs->as.app.arg, bv_map);

        case FORALL_EXPRESSION: {
            Expression *bv_l = lhs->as.forall.bound_variable;
            Expression *bv_r = rhs->as.forall.bound_variable;
            if (!conversion_derivable(get_expression_type(bv_l), get_expression_type(bv_r),
                                      bv_map)) {
                return false;
            }

            map_set(bv_map, bv_l, bv_r);
            bool result = conversion_derivable(lhs->as.forall.body, rhs->as.forall.body, bv_map);
            map_del(bv_map, bv_l);
            return result;
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv_l = lhs->as.lambda.bound_variable;
            Expression *bv_r = rhs->as.lambda.bound_variable;
            if (!conversion_derivable(get_expression_type(bv_l), get_expression_type(bv_r),
                                      bv_map)) {
                return false;
            }

            map_set(bv_map, bv_l, bv_r);
            bool result = conversion_derivable(lhs->as.lambda.body, rhs->as.lambda.body, bv_map);
            map_del(bv_map, bv_l);
            return result;
        }

        case MATCH_EXPRESSION: {
            if (!conversion_derivable(lhs->as.match.scrutinee, rhs->as.match.scrutinee, bv_map)) {
                return false;
            }
            if (lhs->as.match.branch_count != rhs->as.match.branch_count) {
                return false;
            }

            for (int i = 0; i < lhs->as.match.branch_count; i++) {
                MatchBranch *bl = lhs->as.match.branches[i];
                MatchBranch *br = rhs->as.match.branches[i];

                if (!conversion_derivable(bl->constructor, br->constructor, bv_map)) {
                    return false;
                }
                if (bl->pattern_var_count != br->pattern_var_count) {
                    return false;
                }

                bool ok = true;
                int mapped = 0;
                for (int j = 0; j < bl->pattern_var_count; j++) {
                    if (!conversion_derivable(get_expression_type(bl->pattern_variables[j]),
                                              get_expression_type(br->pattern_variables[j]),
                                              bv_map)) {
                        ok = false;
                        break;
                    }
                    map_set(bv_map, bl->pattern_variables[j], br->pattern_variables[j]);
                    mapped++;
                }

                bool result = ok && conversion_derivable(bl->body, br->body, bv_map);

                for (int j = 0; j < mapped; j++) {
                    map_del(bv_map, bl->pattern_variables[j]);
                }
                if (!result) {
                    return false;
                }
            }
            return true;
        }

        case FIX_EXPRESSION: {
            if (lhs->as.fix.arg_count != rhs->as.fix.arg_count ||
                lhs->as.fix.decreasing_arg_index != rhs->as.fix.decreasing_arg_index) {
                return false;
            }
            if (!conversion_derivable(get_expression_type(lhs->as.fix.recursive_var),
                                      get_expression_type(rhs->as.fix.recursive_var), bv_map)) {
                return false;
            }

            map_set(bv_map, lhs->as.fix.recursive_var, rhs->as.fix.recursive_var);
            for (int i = 0; i < lhs->as.fix.arg_count; i++) {
                if (!conversion_derivable(get_expression_type(lhs->as.fix.args[i]),
                                          get_expression_type(rhs->as.fix.args[i]), bv_map)) {
                    for (int j = 0; j < i; j++) {
                        map_del(bv_map, lhs->as.fix.args[j]);
                    }
                    map_del(bv_map, lhs->as.fix.recursive_var);
                    return false;
                }
                map_set(bv_map, lhs->as.fix.args[i], rhs->as.fix.args[i]);
            }

            bool result = conversion_derivable(lhs->as.fix.body, rhs->as.fix.body, bv_map);

            for (int i = 0; i < lhs->as.fix.arg_count; i++) {
                map_del(bv_map, lhs->as.fix.args[i]);
            }
            map_del(bv_map, lhs->as.fix.recursive_var);
            return result;
        }
    }

    return false;
}

static bool conversion_derivable_in_context(Context *context, Expression *lhs, Expression *rhs) {
    if (!context || !valid_in_context(lhs, context) || !valid_in_context(rhs, context)) {
        return false;
    }

    Map *bv_map = map_new_with_capacity(8);
    bool result = conversion_derivable(lhs, rhs, bv_map);
    map_free(bv_map);
    return result;
}

static ConversionRule conversion_top_rule(Expression *lhs, Expression *rhs) {
    Expression *lhs_norm = conversion_whnf(lhs);
    Expression *rhs_norm = conversion_whnf(rhs);

    if (lhs == rhs) {
        return CONVERSION_REFL;
    }
    if (lhs_norm != lhs || rhs_norm != rhs) {
        return CONVERSION_NORMALIZE;
    }

    switch (lhs_norm->tag) {
        case APP_EXPRESSION:
            return CONVERSION_APP;
        case FORALL_EXPRESSION:
            return CONVERSION_FORALL;
        case LAMBDA_EXPRESSION:
            return CONVERSION_LAMBDA;
        case MATCH_EXPRESSION:
            return CONVERSION_MATCH;
        case FIX_EXPRESSION:
            return CONVERSION_FIX;
        default:
            return CONVERSION_REFL;
    }
}

static bool conversion_cached_holds(Expression *lhs, Expression *rhs) {
    bool cacheable = !lhs->has_evar && !rhs->has_evar;

    if (cacheable) {
        if (g_conversion_cache == NULL) {
            g_conversion_cache = map_new_with_capacity(64);
        }
        Map *inner = (Map *)map_get(g_conversion_cache, lhs);
        if (inner != NULL) {
            void *cached = map_get(inner, rhs);
            if (cached == CONV_TRUE) return true;
            if (cached == CONV_FALSE) return false;
        }
    }

    bool result = congruence(lhs, rhs);
    if (result) {
        if (cacheable) {
            Map *inner = (Map *)map_get(g_conversion_cache, lhs);
            if (inner == NULL) {
                inner = map_new_with_capacity(4);
                map_set(g_conversion_cache, lhs, inner);
            }
            map_set(inner, rhs, CONV_TRUE);
        }
        return true;
    }

    Map *bv_map = map_new_with_capacity(8);
    result = conversion_derivable(lhs, rhs, bv_map);
    map_free(bv_map);

    if (cacheable) {
        Map *inner = (Map *)map_get(g_conversion_cache, lhs);
        if (inner == NULL) {
            inner = map_new_with_capacity(4);
            map_set(g_conversion_cache, lhs, inner);
        }
        map_set(inner, rhs, result ? CONV_TRUE : CONV_FALSE);
    }

    return result;
}

Conversion *conversion_check_in_context(Context *context, Expression *lhs, Expression *rhs) {
    if (!conversion_derivable_in_context(context, lhs, rhs)) {
        return NULL;
    }

    Context *min_context = conversion_min_context(lhs, rhs);
    if (!min_context || !context_is_ancestor(min_context, context)) {
        return NULL;
    }

    ConversionRule rule = conversion_top_rule(lhs, rhs);
    return conversion_alloc(rule, min_context, lhs, rhs);
}

bool conversion_holds_in_context(Context *context, Expression *lhs, Expression *rhs) {
    if (!context || !valid_in_context(lhs, context) || !valid_in_context(rhs, context)) {
        return false;
    }
    if (lhs == rhs) {
        return true;
    }

    return conversion_cached_holds(lhs, rhs);
}

ConversionRule conversion_rule(Conversion *conv) { return conv->rule; }

Context *conversion_context(Conversion *conv) { return conv->context; }

Expression *conversion_lhs(Conversion *conv) { return conv->lhs; }

Expression *conversion_rhs(Conversion *conv) { return conv->rhs; }

bool conversion_valid_in_context(Conversion *conv, Context *context) {
    return conv && context && context_is_ancestor(conv->context, context);
}

void conversion_free(Conversion *conv) {
    if (!conv) {
        return;
    }

    conversion_free(conv->left);
    conversion_free(conv->right);
    free(conv);
}
