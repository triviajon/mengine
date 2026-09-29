#include "src/kernel/type_compat.h"

#include <stdlib.h>

#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/context.h"
#include "src/kernel/normalize.h"

typedef struct {
    Map *bv_map;       // bound-variable renaming
    int bound_count;   // entries currently in bv_map
    LinearMap *holes;  // output map: hole -> concrete value discovered during traversal
    Map *compatible;   // expected -> Map of the actuals already found compatible with it
} CompatState;

static bool _open_compat(Expression *expected, Expression *actual, CompatState *st);

static void bind(CompatState *st, Expression *expected_var, Expression *actual_var) {
    map_set(st->bv_map, expected_var, actual_var);
    st->bound_count++;
}

static void unbind(CompatState *st, Expression *expected_var) {
    map_del(st->bv_map, expected_var);
    st->bound_count--;
}

/**
 * Whether actual matches expected up to the bound-variable renaming st->bv_map. A hole on either
 * side matches any term; the term is recorded for it in st->holes, and the hole is not filled.
 */
static bool _open_compat_uncached(Expression *expected, Expression *actual, CompatState *st) {
    expected = normalize_whnf(expected);
    actual = normalize_whnf(actual);

    if (!expected || !actual) {
        return false;
    }
    if (expected == actual) {
        return true;
    }

    if (expected->tag == HOLE_EXPRESSION) {
        Expression *already_mapped = linear_map_get(st->holes, expected);
        if (already_mapped != NULL) {
            return _open_compat(already_mapped, actual, st);
        }

        linear_map_set(st->holes, expected, actual);
        return _open_compat(get_expression_type(expected), get_expression_type(actual), st);
    }

    if (actual->tag == HOLE_EXPRESSION) {
        // Symmetric to the expected-side case
        Expression *already_mapped = linear_map_get(st->holes, actual);
        if (already_mapped != NULL) {
            return _open_compat(expected, already_mapped, st);
        }
        linear_map_set(st->holes, actual, expected);
        return _open_compat(get_expression_type(expected), get_expression_type(actual), st);
    }

    if (expected->tag != actual->tag) {
        return false;
    }

    switch (expected->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return true;

        case VAR_EXPRESSION:
            return ((expected == actual) || (map_get(st->bv_map, expected) == actual)) != 0;

        case APP_EXPRESSION:
            return (_open_compat(expected->as.app.func, actual->as.app.func, st) &&
                    _open_compat(expected->as.app.arg, actual->as.app.arg, st)) != 0;

        case FORALL_EXPRESSION: {
            Expression *bv_e = expected->as.forall.bound_variable;
            Expression *bv_a = actual->as.forall.bound_variable;
            Expression *expected_domain = get_expression_type(bv_e);
            Expression *actual_domain = get_expression_type(bv_a);

            bool domain_ok = (expected_domain->tag == PROP_EXPRESSION &&
                              actual_domain->tag == TYPE_EXPRESSION) ||
                             _open_compat(expected_domain, actual_domain, st);
            if (!domain_ok) {
                return false;
            }

            bind(st, bv_e, bv_a);
            bool result = _open_compat(expected->as.forall.body, actual->as.forall.body, st);
            unbind(st, bv_e);
            return result;
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv_e = expected->as.lambda.bound_variable;
            Expression *bv_a = actual->as.lambda.bound_variable;
            if (!_open_compat(get_expression_type(bv_e), get_expression_type(bv_a), st)) {
                return false;
            }

            bind(st, bv_e, bv_a);
            bool result = _open_compat(expected->as.lambda.body, actual->as.lambda.body, st);
            unbind(st, bv_e);
            return result;
        }

        case MATCH_EXPRESSION: {
            if (!_open_compat(expected->as.match.scrutinee, actual->as.match.scrutinee, st)) {
                return false;
            }
            if (expected->as.match.branch_count != actual->as.match.branch_count) {
                return false;
            }
            for (int i = 0; i < expected->as.match.branch_count; i++) {
                MatchBranch *be = expected->as.match.branches[i];
                MatchBranch *ba = actual->as.match.branches[i];
                if (!_open_compat(be->constructor, ba->constructor, st)) {
                    return false;
                }
                if (be->pattern_var_count != ba->pattern_var_count) {
                    return false;
                }

                bool ok = true;
                int mapped = 0;
                for (int j = 0; j < be->pattern_var_count; j++) {
                    if (!_open_compat(get_expression_type(be->pattern_variables[j]),
                                      get_expression_type(ba->pattern_variables[j]), st)) {
                        ok = false;
                        break;
                    }
                    bind(st, be->pattern_variables[j], ba->pattern_variables[j]);
                    mapped++;
                }

                bool body_result = ok && _open_compat(be->body, ba->body, st);
                for (int j = 0; j < mapped; j++) {
                    unbind(st, be->pattern_variables[j]);
                }
                if (!body_result) {
                    return false;
                }
            }
            return true;
        }

        case FIX_EXPRESSION: {
            Expression *rv_e = expected->as.fix.recursive_var;
            Expression *rv_a = actual->as.fix.recursive_var;
            if (expected->as.fix.arg_count != actual->as.fix.arg_count) {
                return false;
            }
            if (expected->as.fix.decreasing_arg_index != actual->as.fix.decreasing_arg_index) {
                return false;
            }
            if (!_open_compat(get_expression_type(rv_e), get_expression_type(rv_a), st)) {
                return false;
            }

            bind(st, rv_e, rv_a);
            bool ok = true;
            int mapped = 0;
            for (int i = 0; i < expected->as.fix.arg_count; i++) {
                if (!_open_compat(get_expression_type(expected->as.fix.args[i]),
                                  get_expression_type(actual->as.fix.args[i]), st)) {
                    ok = false;
                    break;
                }
                bind(st, expected->as.fix.args[i], actual->as.fix.args[i]);
                mapped++;
            }

            bool result = ok && _open_compat(expected->as.fix.body, actual->as.fix.body, st);
            for (int i = 0; i < mapped; i++) {
                unbind(st, expected->as.fix.args[i]);
            }
            unbind(st, rv_e);
            return result;
        }

        default:
            return false;
    }
}

// _open_compat_uncached, remembering in st->compatible the pairs found compatible while no binder
// is open.
static bool _open_compat(Expression *expected, Expression *actual, CompatState *st) {
    Map *seen = st->bound_count == 0 ? map_get(st->compatible, expected) : NULL;
    if (seen && map_get(seen, actual)) {
        return true;
    }
    bool result = _open_compat_uncached(expected, actual, st);
    if (result && st->bound_count == 0) {
        if (!seen) {
            seen = map_new();
            map_set(st->compatible, expected, seen);
        }
        map_set(seen, actual, actual);
    }
    return result;
}

static void free_actuals(void *key, void *value, void *ud) {
    (void)key;
    (void)ud;
    map_free((Map *)value);
}

bool open_types_compatible_collecting_in_context(Context *context, Expression *expected,
                                                 Expression *actual, LinearMap *holes) {
    if (!context || !expected || !actual || !holes) {
        return false;
    }
    if (!valid_in_context(expected, context) || !valid_in_context(actual, context)) {
        return false;
    }
    if (expected == actual) {
        return true;
    }

    CompatState st = {map_new_with_capacity(8), 0, holes, map_new()};
    bool result = _open_compat(expected, actual, &st);
    map_free(st.bv_map);
    map_for_each(st.compatible, free_actuals, NULL);
    map_free(st.compatible);
    return result;
}

bool open_types_compatible_in_context(Context *context, Expression *expected, Expression *actual) {
    LinearMap *holes = linear_map_new();
    bool result = open_types_compatible_collecting_in_context(context, expected, actual, holes);
    linear_map_clear_free(holes);
    return result;
}
