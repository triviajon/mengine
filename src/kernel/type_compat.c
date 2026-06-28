#include "src/kernel/type_compat.h"

#include <stdlib.h>

#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/context.h"
#include "src/kernel/normalize.h"

/**
 * Checks whether two types are compatible under hole-aware matching. When a
 * HOLE_EXPRESSION appears on the expected side, it is treated as a unification
 * variable and recorded in holes instead of being filled.
 *
 * bv_map - bound-variable renaming
 * holes - output map: hole -> concrete value discovered during traversal.
 *
 * No holes are filled "side-effect"-fully.
 */
static bool _open_compat(Expression *expected, Expression *actual, Map *bv_map, LinearMap *holes) {
    expected = normalize_whnf(expected);
    actual = normalize_whnf(actual);

    if (!expected || !actual) {
        return false;
    }
    if (expected == actual) {
        return true;
    }

    if (expected->tag == HOLE_EXPRESSION) {
        Expression *already_mapped = linear_map_get(holes, expected);
        if (already_mapped != NULL) {
            return _open_compat(already_mapped, actual, bv_map, holes);
        }

        linear_map_set(holes, expected, actual);
        return _open_compat(get_expression_type(expected), get_expression_type(actual), bv_map,
                            holes);
    }

    if (actual->tag == HOLE_EXPRESSION) {
        // Symmetric to the expected-side case: a hole on the actual (term-type) side is
        // an unsolved evar that must be instantiated to `expected` for the term's type
        // to match. This occurs when applying a function whose result type is a
        // beta-redex that whnf-reduces to a binder (e.g. an eliminator's `motive y0`
        // with a quantified motive): the index `y0` cannot be solved by spine
        // unification and ends up embedded in the term type as a hole. Recording the
        // assignment lets fill_hole cascade-fill it instead of leaving a stray goal.
        Expression *already_mapped = linear_map_get(holes, actual);
        if (already_mapped != NULL) {
            return _open_compat(expected, already_mapped, bv_map, holes);
        }
        linear_map_set(holes, actual, expected);
        return _open_compat(get_expression_type(expected), get_expression_type(actual), bv_map,
                            holes);
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
            Expression *expected_domain = get_expression_type(bv_e);
            Expression *actual_domain = get_expression_type(bv_a);

            bool domain_ok = (expected_domain->tag == PROP_EXPRESSION &&
                              actual_domain->tag == TYPE_EXPRESSION) ||
                             _open_compat(expected_domain, actual_domain, bv_map, holes);
            if (!domain_ok) {
                return false;
            }

            map_set(bv_map, bv_e, bv_a);
            bool result =
                _open_compat(expected->as.forall.body, actual->as.forall.body, bv_map, holes);
            map_del(bv_map, bv_e);
            return result;
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv_e = expected->as.lambda.bound_variable;
            Expression *bv_a = actual->as.lambda.bound_variable;
            if (!_open_compat(get_expression_type(bv_e), get_expression_type(bv_a), bv_map,
                              holes)) {
                return false;
            }

            map_set(bv_map, bv_e, bv_a);
            bool result =
                _open_compat(expected->as.lambda.body, actual->as.lambda.body, bv_map, holes);
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

                bool ok = true;
                int mapped = 0;
                for (int j = 0; j < be->pattern_var_count; j++) {
                    if (!_open_compat(get_expression_type(be->pattern_variables[j]),
                                      get_expression_type(ba->pattern_variables[j]), bv_map,
                                      holes)) {
                        ok = false;
                        break;
                    }
                    map_set(bv_map, be->pattern_variables[j], ba->pattern_variables[j]);
                    mapped++;
                }

                bool body_result = ok && _open_compat(be->body, ba->body, bv_map, holes);
                for (int j = 0; j < mapped; j++) {
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
            if (expected->as.fix.arg_count != actual->as.fix.arg_count) {
                return false;
            }
            if (expected->as.fix.decreasing_arg_index != actual->as.fix.decreasing_arg_index) {
                return false;
            }
            if (!_open_compat(get_expression_type(rv_e), get_expression_type(rv_a), bv_map,
                              holes)) {
                return false;
            }

            map_set(bv_map, rv_e, rv_a);
            bool ok = true;
            int mapped = 0;
            for (int i = 0; i < expected->as.fix.arg_count; i++) {
                if (!_open_compat(get_expression_type(expected->as.fix.args[i]),
                                  get_expression_type(actual->as.fix.args[i]), bv_map, holes)) {
                    ok = false;
                    break;
                }
                map_set(bv_map, expected->as.fix.args[i], actual->as.fix.args[i]);
                mapped++;
            }

            bool result =
                ok && _open_compat(expected->as.fix.body, actual->as.fix.body, bv_map, holes);
            for (int i = 0; i < mapped; i++) {
                map_del(bv_map, expected->as.fix.args[i]);
            }
            map_del(bv_map, rv_e);
            return result;
        }

        default:
            return false;
    }
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

    Map *bv_map = map_new_with_capacity(8);
    bool result = _open_compat(expected, actual, bv_map, holes);
    map_free(bv_map);
    return result;
}

bool open_types_compatible_in_context(Context *context, Expression *expected, Expression *actual) {
    LinearMap *holes = linear_map_new();
    bool result = open_types_compatible_collecting_in_context(context, expected, actual, holes);
    linear_map_clear_free(holes);
    return result;
}
