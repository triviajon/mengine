#include "src/kernel/definitional_equal.h"

#include <stdlib.h>

#include "src/common/linear_map.h"
#include "src/kernel/normalize.h"

static bool _defeq(Expression *a, Expression *b, LinearMap *mapping) {
    // Reduce both sides to WHNF before inspecting structure
    a = normalize_whnf(a);
    b = normalize_whnf(b);

    if (a == b) {
        return true;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return true;

        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            return (a == b) || (linear_map_get(mapping, a) == b);

        case APP_EXPRESSION:
            return _defeq(a->as.app.func, b->as.app.func, mapping) &&
                   _defeq(a->as.app.arg, b->as.app.arg, mapping);

        case FORALL_EXPRESSION:
            linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            return _defeq(a->as.forall.body, b->as.forall.body, mapping);

        case LAMBDA_EXPRESSION:
            linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            return _defeq(a->as.lambda.body, b->as.lambda.body, mapping);

        case MATCH_EXPRESSION: {
            if (!_defeq(a->as.match.scrutinee, b->as.match.scrutinee, mapping)) {
                return false;
            }

            if (a->as.match.branch_count != b->as.match.branch_count) {
                return false;
            }

            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *ba = a->as.match.branches[i];
                MatchBranch *bb = b->as.match.branches[i];

                if (!_defeq(ba->constructor, bb->constructor, mapping)) {
                    return false;
                }

                if (ba->pattern_var_count != bb->pattern_var_count) {
                    return false;
                }

                for (int j = 0; j < ba->pattern_var_count; j++) {
                    linear_map_set(mapping, ba->pattern_variables[j], bb->pattern_variables[j]);
                }

                if (!_defeq(ba->body, bb->body, mapping)) {
                    return false;
                }
            }
            return true;
        }

        case FIX_EXPRESSION:
            linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);

            if (a->as.fix.arg_count != b->as.fix.arg_count) {
                return false;
            }

            if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                return false;
            }

            for (int i = 0; i < a->as.fix.arg_count; i++) {
                linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }

            return _defeq(a->as.fix.body, b->as.fix.body, mapping);
    }

    return false;
}

bool definitional_equal(Expression *a, Expression *b) {
    if (a == b) return true;
    LinearMap *mapping = linear_map_new();
    bool result = _defeq(a, b, mapping);
    free(mapping->items);
    free(mapping);
    return result;
}

/**
 * Checks whether two types are compatible under open-term (hole-aware)
 * matching. Identical to definitional_equal except: when a HOLE_EXPRESSION
 * appears on the *expected* (left-hand) side, it is treated as an unification
 * variable.
 *
 * `bv_map`  — bound-variable renaming (FORALL/LAMBDA/MATCH/FIX binders),
 *             internal to the traversal.
 * `holes`   — output map: hole → concrete value discovered during traversal.
 *             Caller-supplied; entries are appended (never cleared by this fn).
 *
 * First occurrence of a hole `?v` on the expected side:
 *   - records holes[?v] = actual_subterm
 *   - checks definitional_equal(type(?v), type(actual_subterm))
 *
 * Subsequent occurrences of the same hole `?v`:
 *   - looks up holes[?v] to get the previously recorded actual_subterm
 *   - checks definitional_equal(recorded_actual, current_actual)
 *
 * This is a pure predicate — no holes are filled side-effectfully.
 */
static bool _open_compat(Expression *expected, Expression *actual,
                         LinearMap *bv_map, LinearMap *holes) {
    expected = normalize_whnf(expected);
    actual = normalize_whnf(actual);

    if (expected == actual) return true;

    if (expected->tag == HOLE_EXPRESSION) {
        Expression *already_mapped = linear_map_get(holes, expected);
        if (already_mapped != NULL) {
            // Seen this hole before — check the new actual is defeq to the
            // value we already committed this hole to.
            return definitional_equal(already_mapped, actual);
        }
        // First encounter: record the assignment and check the type is compatible.
        linear_map_set(holes, expected, actual);
        return definitional_equal(get_expression_type(expected), get_expression_type(actual));
    }

    if (actual->tag == HOLE_EXPRESSION) {
        return definitional_equal(get_expression_type(expected), get_expression_type(actual));
    }

    if (expected->tag != actual->tag) return false;

    switch (expected->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return true;

        case VAR_EXPRESSION:
            return (expected == actual) || (linear_map_get(bv_map, expected) == actual);

        case APP_EXPRESSION:
            return _open_compat(expected->as.app.func, actual->as.app.func, bv_map, holes) &&
                   _open_compat(expected->as.app.arg, actual->as.app.arg, bv_map, holes);

        case FORALL_EXPRESSION:
            linear_map_set(bv_map, expected->as.forall.bound_variable,
                           actual->as.forall.bound_variable);
            return _open_compat(expected->as.forall.body, actual->as.forall.body, bv_map, holes);

        case LAMBDA_EXPRESSION:
            linear_map_set(bv_map, expected->as.lambda.bound_variable,
                           actual->as.lambda.bound_variable);
            return _open_compat(expected->as.lambda.body, actual->as.lambda.body, bv_map, holes);

        case MATCH_EXPRESSION: {
            if (!_open_compat(expected->as.match.scrutinee, actual->as.match.scrutinee,
                              bv_map, holes))
                return false;
            if (expected->as.match.branch_count != actual->as.match.branch_count) return false;
            for (int i = 0; i < expected->as.match.branch_count; i++) {
                MatchBranch *be = expected->as.match.branches[i];
                MatchBranch *ba = actual->as.match.branches[i];
                if (!_open_compat(be->constructor, ba->constructor, bv_map, holes)) return false;
                if (be->pattern_var_count != ba->pattern_var_count) return false;
                for (int j = 0; j < be->pattern_var_count; j++)
                    linear_map_set(bv_map, be->pattern_variables[j], ba->pattern_variables[j]);
                if (!_open_compat(be->body, ba->body, bv_map, holes)) return false;
            }
            return true;
        }

        case FIX_EXPRESSION:
            linear_map_set(bv_map, expected->as.fix.recursive_var, actual->as.fix.recursive_var);
            if (expected->as.fix.arg_count != actual->as.fix.arg_count) return false;
            if (expected->as.fix.decreasing_arg_index != actual->as.fix.decreasing_arg_index)
                return false;
            for (int i = 0; i < expected->as.fix.arg_count; i++)
                linear_map_set(bv_map, expected->as.fix.args[i], actual->as.fix.args[i]);
            return _open_compat(expected->as.fix.body, actual->as.fix.body, bv_map, holes);

        default:
            return false;
    }
}

bool open_types_compatible_collecting(Expression *expected, Expression *actual,
                                      LinearMap *holes) {
    if (expected == actual) return true;
    LinearMap *bv_map = linear_map_new();
    bool result = _open_compat(expected, actual, bv_map, holes);
    linear_map_clear_free(bv_map);
    return result;
}

bool open_types_compatible(Expression *expected, Expression *actual) {
    LinearMap *holes = linear_map_new();
    bool result = open_types_compatible_collecting(expected, actual, holes);
    linear_map_clear_free(holes);
    return result;
}