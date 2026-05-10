#include "src/kernel/conversion.h"

#include <stdint.h>
#include <stdlib.h>

#include "src/common/map.h"
#include "src/kernel/context.h"
#include "src/kernel/normalize.h"

typedef struct ConversionList {
    Conversion *conv;
    struct ConversionList *next;
} ConversionList;

struct Conversion {
    ConversionRule rule;
    Context *context;
    Expression *lhs;
    Expression *rhs;
    Expression *join;
    Conversion *left;
    Conversion *right;
    ConversionList *children;
};

static Context *conversion_common_context(Expression *lhs, Expression *rhs) {
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

static bool conversion_add_child(Conversion *parent, Conversion *child) {
    if (!parent || !child) {
        return false;
    }

    ConversionList *node = malloc(sizeof(ConversionList));
    if (!node) {
        return false;
    }
    node->conv = child;
    node->next = parent->children;
    parent->children = node;
    return true;
}

Conversion *conversion_refl(Context *context, Expression *expr) {
    return conversion_alloc(CONVERSION_REFL, context, expr, expr);
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

static Map *g_conversion_cache = NULL;

static void conversion_cache_free_inner(void *inner) { map_free((Map *)inner); }

void conversion_cache_clear(void) {
    if (g_conversion_cache == NULL) {
        return;
    }

    map_clear_apply_free(g_conversion_cache, conversion_cache_free_inner);
    g_conversion_cache = NULL;
}

static Conversion *conversion_normalize(Context *ctx, Expression *from, Expression *to) {
    Conversion *conv = conversion_alloc(CONVERSION_NORMALIZE, ctx, from, to);
    if (conv) {
        conv->join = to;
    }
    return conv;
}

static Conversion *conversion_compose_normalized(Context *ctx, Expression *lhs,
                                                 Expression *lhs_norm, Expression *rhs_norm,
                                                 Expression *rhs, Conversion *middle) {
    if (!middle) {
        return NULL;
    }

    Conversion *left = (lhs == lhs_norm) ? conversion_refl(ctx, lhs)
                                         : conversion_normalize(ctx, lhs, lhs_norm);
    Conversion *right = (rhs == rhs_norm) ? conversion_refl(ctx, rhs)
                                          : conversion_normalize(ctx, rhs, rhs_norm);
    Conversion *right_sym = conversion_sym(right);
    if (!left || !right || !right_sym) {
        conversion_free(left);
        conversion_free(middle);
        conversion_free(right_sym);
        if (!right_sym) {
            conversion_free(right);
        }
        return NULL;
    }

    Conversion *prefix = conversion_trans(left, middle);
    if (!prefix) {
        conversion_free(left);
        conversion_free(middle);
        conversion_free(right_sym);
        return NULL;
    }
    Conversion *result = conversion_trans(prefix, right_sym);
    if (!result) {
        conversion_free(prefix);
        conversion_free(right_sym);
        return NULL;
    }
    return result;
}

static Conversion *_conversion_check(Expression *lhs, Expression *rhs, Map *bv_map);

static Conversion *conversion_structural(Context *ctx, Expression *lhs, Expression *rhs,
                                         Map *bv_map) {
    if (lhs == rhs) {
        return conversion_refl(ctx, lhs);
    }

    if (lhs->tag != rhs->tag) {
        return NULL;
    }

    switch (lhs->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return conversion_alloc(CONVERSION_REFL, ctx, lhs, rhs);

        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            if (map_get(bv_map, lhs) == rhs) {
                return conversion_alloc(CONVERSION_REFL, ctx, lhs, rhs);
            }
            return NULL;

        case APP_EXPRESSION: {
            Conversion *func =
                _conversion_check(lhs->as.app.func, rhs->as.app.func, bv_map);
            if (!func) {
                return NULL;
            }
            Conversion *arg = _conversion_check(lhs->as.app.arg, rhs->as.app.arg, bv_map);
            if (!arg) {
                conversion_free(func);
                return NULL;
            }
            Conversion *conv = conversion_alloc(CONVERSION_APP, ctx, lhs, rhs);
            if (!conv || !conversion_add_child(conv, func) || !conversion_add_child(conv, arg)) {
                conversion_free(conv);
                conversion_free(func);
                conversion_free(arg);
                return NULL;
            }
            return conv;
        }

        case FORALL_EXPRESSION: {
            Expression *bv_l = lhs->as.forall.bound_variable;
            Expression *bv_r = rhs->as.forall.bound_variable;
            map_set(bv_map, bv_l, bv_r);
            Conversion *body = _conversion_check(lhs->as.forall.body, rhs->as.forall.body, bv_map);
            map_del(bv_map, bv_l);
            if (!body) {
                return NULL;
            }
            Conversion *conv = conversion_alloc(CONVERSION_FORALL, ctx, lhs, rhs);
            if (!conv || !conversion_add_child(conv, body)) {
                conversion_free(conv);
                conversion_free(body);
                return NULL;
            }
            return conv;
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv_l = lhs->as.lambda.bound_variable;
            Expression *bv_r = rhs->as.lambda.bound_variable;
            map_set(bv_map, bv_l, bv_r);
            Conversion *body = _conversion_check(lhs->as.lambda.body, rhs->as.lambda.body, bv_map);
            map_del(bv_map, bv_l);
            if (!body) {
                return NULL;
            }
            Conversion *conv = conversion_alloc(CONVERSION_LAMBDA, ctx, lhs, rhs);
            if (!conv || !conversion_add_child(conv, body)) {
                conversion_free(conv);
                conversion_free(body);
                return NULL;
            }
            return conv;
        }

        case MATCH_EXPRESSION: {
            if (lhs->as.match.branch_count != rhs->as.match.branch_count) {
                return NULL;
            }

            Conversion *conv = conversion_alloc(CONVERSION_MATCH, ctx, lhs, rhs);
            if (!conv) {
                return NULL;
            }

            Conversion *scrut =
                _conversion_check(lhs->as.match.scrutinee, rhs->as.match.scrutinee, bv_map);
            if (!scrut || !conversion_add_child(conv, scrut)) {
                conversion_free(scrut);
                conversion_free(conv);
                return NULL;
            }

            for (int i = 0; i < lhs->as.match.branch_count; i++) {
                MatchBranch *bl = lhs->as.match.branches[i];
                MatchBranch *br = rhs->as.match.branches[i];
                if (bl->pattern_var_count != br->pattern_var_count) {
                    conversion_free(conv);
                    return NULL;
                }

                Conversion *ctor = _conversion_check(bl->constructor, br->constructor, bv_map);
                if (!ctor || !conversion_add_child(conv, ctor)) {
                    conversion_free(ctor);
                    conversion_free(conv);
                    return NULL;
                }

                for (int j = 0; j < bl->pattern_var_count; j++) {
                    map_set(bv_map, bl->pattern_variables[j], br->pattern_variables[j]);
                }
                Conversion *body = _conversion_check(bl->body, br->body, bv_map);
                for (int j = 0; j < bl->pattern_var_count; j++) {
                    map_del(bv_map, bl->pattern_variables[j]);
                }
                if (!body || !conversion_add_child(conv, body)) {
                    conversion_free(body);
                    conversion_free(conv);
                    return NULL;
                }
            }
            return conv;
        }

        case FIX_EXPRESSION: {
            if (lhs->as.fix.arg_count != rhs->as.fix.arg_count ||
                lhs->as.fix.decreasing_arg_index != rhs->as.fix.decreasing_arg_index) {
                return NULL;
            }

            map_set(bv_map, lhs->as.fix.recursive_var, rhs->as.fix.recursive_var);
            for (int i = 0; i < lhs->as.fix.arg_count; i++) {
                map_set(bv_map, lhs->as.fix.args[i], rhs->as.fix.args[i]);
            }
            Conversion *body = _conversion_check(lhs->as.fix.body, rhs->as.fix.body, bv_map);
            for (int i = 0; i < lhs->as.fix.arg_count; i++) {
                map_del(bv_map, lhs->as.fix.args[i]);
            }
            map_del(bv_map, lhs->as.fix.recursive_var);
            if (!body) {
                return NULL;
            }
            Conversion *conv = conversion_alloc(CONVERSION_FIX, ctx, lhs, rhs);
            if (!conv || !conversion_add_child(conv, body)) {
                conversion_free(conv);
                conversion_free(body);
                return NULL;
            }
            return conv;
        }
    }

    return NULL;
}

static Conversion *_conversion_check(Expression *lhs, Expression *rhs, Map *bv_map) {
    Context *ctx = conversion_common_context(lhs, rhs);
    if (!ctx) {
        ctx = get_expression_context(lhs);
    }

    Expression *lhs_norm = normalize_whnf(lhs);
    Expression *rhs_norm = normalize_whnf(rhs);
    if (!lhs_norm || !rhs_norm) {
        return NULL;
    }

    Conversion *middle = conversion_structural(ctx, lhs_norm, rhs_norm, bv_map);
    return conversion_compose_normalized(ctx, lhs, lhs_norm, rhs_norm, rhs, middle);
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

    Conversion *conv = conversion_check(lhs, rhs);
    bool result = conv != NULL;
    conversion_free(conv);

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

Conversion *conversion_check(Expression *lhs, Expression *rhs) {
    if (!lhs || !rhs) {
        return NULL;
    }
    Map *bv_map = map_new_with_capacity(8);
    Conversion *conv = _conversion_check(lhs, rhs, bv_map);
    map_free(bv_map);
    return conv;
}

Conversion *conversion_check_in_context(Context *context, Expression *lhs, Expression *rhs) {
    if (!context || !valid_in_context(lhs, context) || !valid_in_context(rhs, context)) {
        return NULL;
    }
    Conversion *conv = conversion_check(lhs, rhs);
    if (!conversion_valid_in_context(conv, context)) {
        conversion_free(conv);
        return NULL;
    }
    return conv;
}

bool conversion_holds(Expression *lhs, Expression *rhs) {
    if (lhs == rhs) {
        return true;
    }
    return conversion_cached_holds(lhs, rhs);
}

bool conversion_holds_in_context(Context *context, Expression *lhs, Expression *rhs) {
    if (!context || !valid_in_context(lhs, context) || !valid_in_context(rhs, context)) {
        return false;
    }
    if (lhs == rhs) {
        return true;
    }

    /*
     * Convertibility is stable under weakening: once lhs and rhs are known to be
     * valid in context, the result does not depend on the extra bindings in
     * context. Reuse the same pair cache as the context-free compatibility
     * wrapper.
     */
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

    ConversionList *child = conv->children;
    while (child) {
        ConversionList *next = child->next;
        conversion_free(child->conv);
        free(child);
        child = next;
    }

    free(conv);
}
