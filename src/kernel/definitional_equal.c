#include "src/kernel/definitional_equal.h"

#include <stdlib.h>

#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/conversion.h"
#include "src/kernel/normalize.h"

void definitional_equal_cache_clear(void) { conversion_cache_clear(); }

bool definitional_equal(Expression *a, Expression *b) {
    return conversion_holds(a, b);
}

/**
 * Checks whether two types are compatible under hole-aware matching. Identical to
 * definitional_equal except when a HOLE_EXPRESSION appears on the *expected* (left-hand) side, it
 * is treated as an unification variable.
 *
 * bv_map - bound-variable renaming
 * holes - output map: hole -> concrete value discovered during traversal.
 *
 * no holes are filled "side-effect"-fully.
 */
static bool _open_compat(Expression *expected, Expression *actual, Map *bv_map,
                         LinearMap *holes) {
    expected = normalize_whnf(expected);
    actual = normalize_whnf(actual);

    if (expected == actual) {
        return true;
    }

    if (expected->tag == HOLE_EXPRESSION) {
        Expression *already_mapped = linear_map_get(holes, expected);
        if (already_mapped != NULL) {
            // Seen this hole before - check the new actual is defeq to the
            // value we already committed this hole to.
            return conversion_holds(already_mapped, actual);
        }
        // First encounter: record the assignment and check the type is compatible.
        linear_map_set(holes, expected, actual);
        return conversion_holds(get_expression_type(expected), get_expression_type(actual));
    }

    if (actual->tag == HOLE_EXPRESSION) {
        return conversion_holds(get_expression_type(expected), get_expression_type(actual));
    }

    if (expected->tag != actual->tag) {
        return false;
    }

    switch (expected->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return true;

        case VAR_EXPRESSION:
            return ((expected == actual) || (map_get(bv_map, expected) == actual)) != 0;

        case APP_EXPRESSION:
            return (_open_compat(expected->as.app.func, actual->as.app.func, bv_map, holes) &&
                    _open_compat(expected->as.app.arg, actual->as.app.arg, bv_map, holes)) != 0;

        case FORALL_EXPRESSION: {
            Expression *bv_e = expected->as.forall.bound_variable;
            Expression *bv_a = actual->as.forall.bound_variable;
            map_set(bv_map, bv_e, bv_a);
            bool result = _open_compat(expected->as.forall.body, actual->as.forall.body, bv_map, holes);
            map_del(bv_map, bv_e);
            return result;
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv_e = expected->as.lambda.bound_variable;
            Expression *bv_a = actual->as.lambda.bound_variable;
            map_set(bv_map, bv_e, bv_a);
            bool result = _open_compat(expected->as.lambda.body, actual->as.lambda.body, bv_map, holes);
            map_del(bv_map, bv_e);
            return result;
        }

        case MATCH_EXPRESSION: {
            if (!_open_compat(expected->as.match.scrutinee, actual->as.match.scrutinee, bv_map,
                              holes)) {
                return false;
            }
            if (expected->as.match.branch_count != actual->as.match.branch_count) {
                return false;
            }
            for (int i = 0; i < expected->as.match.branch_count; i++) {
                MatchBranch *be = expected->as.match.branches[i];
                MatchBranch *ba = actual->as.match.branches[i];
                if (!_open_compat(be->constructor, ba->constructor, bv_map, holes)) {
                    return false;
                }
                if (be->pattern_var_count != ba->pattern_var_count) {
                    return false;
                }
                for (int j = 0; j < be->pattern_var_count; j++) {
                    map_set(bv_map, be->pattern_variables[j], ba->pattern_variables[j]);
                }
                bool body_result = _open_compat(be->body, ba->body, bv_map, holes);
                for (int j = 0; j < be->pattern_var_count; j++) {
                    map_del(bv_map, be->pattern_variables[j]);
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
            map_set(bv_map, rv_e, rv_a);
            if (expected->as.fix.arg_count != actual->as.fix.arg_count) {
                map_del(bv_map, rv_e);
                return false;
            }
            if (expected->as.fix.decreasing_arg_index != actual->as.fix.decreasing_arg_index) {
                map_del(bv_map, rv_e);
                return false;
            }
            for (int i = 0; i < expected->as.fix.arg_count; i++) {
                map_set(bv_map, expected->as.fix.args[i], actual->as.fix.args[i]);
            }
            bool result = _open_compat(expected->as.fix.body, actual->as.fix.body, bv_map, holes);
            for (int i = 0; i < expected->as.fix.arg_count; i++) {
                map_del(bv_map, expected->as.fix.args[i]);
            }
            map_del(bv_map, rv_e);
            return result;
        }

        default:
            return false;
    }
}

bool open_types_compatible_collecting(Expression *expected, Expression *actual, LinearMap *holes) {
    if (expected == actual) {
        return true;
    }
    Map *bv_map = map_new_with_capacity(8);
    bool result = _open_compat(expected, actual, bv_map, holes);
    map_free(bv_map);
    return result;
}

bool open_types_compatible(Expression *expected, Expression *actual) {
    LinearMap *holes = linear_map_new();
    bool result = open_types_compatible_collecting(expected, actual, holes);
    linear_map_clear_free(holes);
    return result;
}
