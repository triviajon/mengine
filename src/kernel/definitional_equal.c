#include "src/kernel/definitional_equal.h"

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
    if (a == b) {
        return true;
    }

    LinearMap *mapping = linear_map_new();
    bool result = _defeq(a, b, mapping);
    linear_map_clear_free(mapping);
    return result;
}