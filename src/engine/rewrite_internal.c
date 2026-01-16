#include "src/engine/rewrite_internal.h"

#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/engine/unify.h"
#include "src/runtime/core.h"

bool rewrite_is_noop(RewriteResult *rwr) { return rwr->original == rwr->rewritten; }

Expression *_build_reflexivity_proof(Expression *expr, Context *ctx) {
    // The goal is to build a proof of eq type(expr) expr expr.

    Expression *relation_over = get_expression_type(expr);
    Expression *proof =
        init_app_expression_wc(init_app_expression_wc(eq_refl, relation_over, ctx), expr, ctx);

    return proof;
}

Expression *_build_transitivity_proof(RewriteResult *first_rwr, RewriteResult *second_rwr,
                                      Context *ctx) {
    // Given first_rwr : original -> mid with proof pf
    // and second_rwr : mid -> rewritten with proof pg
    // eq_trans : forall (A : Type) (x y z : A), eq A x y -> eq A y z -> eq A x
    // z. build the term "eq_trans relation_over original mid rewritten H1 H2"
    Expression *relation_over = get_expression_type(first_rwr->original);
    Expression *original = first_rwr->original;
    Expression *mid = first_rwr->rewritten;
    Expression *rewritten = second_rwr->rewritten;
    Expression *H1 = first_rwr->original_to_rewritten_proof;
    Expression *H2 = second_rwr->original_to_rewritten_proof;

    Expression *proof = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(init_app_expression_wc(eq_trans, relation_over, ctx),
                                           original, ctx),
                    mid, ctx),
                rewritten, ctx),
            H1, ctx),
        H2, ctx);

    return proof;
}

Expression *_build_app_congruence_proof(RewriteResult *func_rwr, RewriteResult *arg_rwr,
                                        Context *ctx) {
    // Bad_App_Congruence : forall (A B : Type) (f g : A -> B) (x y: A), eq (A
    // -> B) f g -> eq (A) x y -> eq (B) (f x) (g y). if func_rw provides pf :
    // eq (A -> B) f g and arg_rwr provided pg : eq A x y, build the term
    // "Bad_App_Congruence A B f g x y pf pg"

    Expression *A_implies_B = get_app_arg(
        get_app_func(get_app_func(get_expression_type(func_rwr->original_to_rewritten_proof))));

    Expression *A = get_arrow_lhs(A_implies_B);

    Expression *f = func_rwr->original;
    Expression *g = func_rwr->rewritten;

    Expression *x = arg_rwr->original;
    Expression *y = arg_rwr->rewritten;

    Expression *H1 = func_rwr->original_to_rewritten_proof;
    Expression *H2 = arg_rwr->original_to_rewritten_proof;

    Expression *f_x = init_app_expression_wc(f, x, ctx);
    Expression *B = get_expression_type(f_x);

    Expression *proof = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(
                            init_app_expression_wc(
                                init_app_expression_wc(Bad_App_Congruence, A, ctx), B, ctx),
                            f, ctx),
                        g, ctx),
                    x, ctx),
                y, ctx),
            H1, ctx),
        H2, ctx);

    return proof;
}

RewriteResult *init_rewrite_result(Expression *original, Expression *rewritten,
                                   DoublyLinkedList *new_goals,
                                   Expression *original_to_rewritten_proof) {
    RewriteResult *rwr = malloc(sizeof(RewriteResult));
    if (!rwr) {
        return NULL;
    }

    rwr->original = original;
    rwr->rewritten = rewritten;
    rwr->new_goals = new_goals;
    rwr->original_to_rewritten_proof = original_to_rewritten_proof;

    return rwr;
}

void free_rewrite_result(RewriteResult *rwr) {
    if (!rwr) {
        return;
    }
    free(rwr);
}

RewriteResult *rewrite_head(Expression *mid, Expression *lemma, Context *context,
                            bool allow_unresolved_bindings) {
    // This part of rewriting is literally just a call to the apply tactic.
    // We're creating a hole with expected return type `mid` and defining
    // context `context`, and attempting to apply the lemma to it. Expression
    // *hole = init_hole_expression("Rewrite_Hole", mid, context);

    // // TODO: Temporary, until the hole fill bug is fixed.
    // Expression *holder =
    // init_lambda_expression_wc(init_var_expression_wc("t",
    // init_type_expression(), context),
    //      hole, context);

    // TODO: We shouldn't need to use this function
    UnificationResult *unif_result = bad_unify_for_eq(context, lemma, mid);

    if (!unif_result) {
        return init_rewrite_result(mid, mid, dll_create(), _build_reflexivity_proof(mid, context));
    }

    if (!allow_unresolved_bindings && dll_len(unif_result->new_goals) > 0) {
        free_unification_result(unif_result);
        return init_rewrite_result(mid, mid, dll_create(), _build_reflexivity_proof(mid, context));
    }

    Expression *proof = unif_result->lemma_instantiation;
    Expression *proof_type = get_expression_type(proof);
    if (!congruence(_get_lhs_eq(proof_type), mid)) {
        free_unification_result(unif_result);
        return init_rewrite_result(mid, mid, dll_create(), _build_reflexivity_proof(mid, context));
    }

    DoublyLinkedList *goals = unif_result->new_goals;
    free_unification_result(unif_result);

    return init_rewrite_result(mid, _get_rhs_eq(proof_type), goals, proof);
}

RewriteResult *_rewrite(Expression *expr, Expression *lemma, Context *context, Map *cached_rwr,
                        bool allow_unresolved_bindings);

RewriteResult *rewrite_app(Expression *expr, Expression *lemma, Context *context, Map *cached_rwr,
                           bool allow_unresolved_bindings) {
    Expression *func = get_app_func(expr);
    Expression *arg = get_app_arg(expr);

    RewriteResult *rwr_func = _rewrite(func, lemma, context, cached_rwr, allow_unresolved_bindings);
    RewriteResult *rwr_arg = _rewrite(arg, lemma, context, cached_rwr, allow_unresolved_bindings);

    RewriteResult *mid_rwr = NULL;
    if (rewrite_is_noop(rwr_func) && rewrite_is_noop(rwr_arg)) {
        mid_rwr =
            init_rewrite_result(expr, expr, dll_create(), _build_reflexivity_proof(expr, context));
    } else {
        mid_rwr = init_rewrite_result(
            expr, init_app_expression_wc(rwr_func->rewritten, rwr_arg->rewritten, context),
            dll_merge(rwr_func->new_goals, rwr_arg->new_goals),
            _build_app_congruence_proof(rwr_func, rwr_arg, context));
    }

    RewriteResult *mid_result =
        rewrite_head(mid_rwr->rewritten, lemma, context, allow_unresolved_bindings);

    RewriteResult *final_rwr;
    if (rewrite_is_noop(mid_result)) {
        final_rwr = init_rewrite_result(expr, mid_rwr->rewritten, mid_rwr->new_goals,
                                        mid_rwr->original_to_rewritten_proof);
    } else {
        final_rwr = init_rewrite_result(expr, mid_result->rewritten,
                                        dll_merge(mid_rwr->new_goals, mid_result->new_goals),
                                        _build_transitivity_proof(mid_rwr, mid_result, context));
    }

    return final_rwr;
}

RewriteResult *rewrite_var(Expression *expr, Expression *lemma, Context *context, Map *_,
                           bool allow_unresolved_bindings) {
    RewriteResult *head_rwr = rewrite_head(expr, lemma, context, allow_unresolved_bindings);
    return head_rwr;
}

RewriteResult *_rewrite(Expression *expr, Expression *lemma, Context *context, Map *cached_rwr,
                        bool allow_unresolved_bindings) {
    RewriteResult *result = (RewriteResult *)map_get(cached_rwr, (void *)expr);

    if (result) {
        return result;
    }

    switch (expr->tag) {
        case (APP_EXPRESSION): {
            result = rewrite_app(expr, lemma, context, cached_rwr, allow_unresolved_bindings);
            break;
        }
        case (VAR_EXPRESSION): {
            result = rewrite_var(expr, lemma, context, cached_rwr, allow_unresolved_bindings);
            break;
        }
        default:
            return NULL;
    }

    map_set(cached_rwr, (void *)expr, result);

    return result;
}

RewriteResult *rewrite(Expression *expr, Expression *lemma, Context *context) {
    Map *cached_rwr = map_new();
    RewriteResult *rwr = _rewrite(expr, lemma, context, cached_rwr, false);
    map_del(cached_rwr,
            (void *)expr);  // remove it from the map so we don't accidently free the rwr
    map_clear_free_values(cached_rwr);
    return rwr;
}

RewriteResult *erewrite(Expression *expr, Expression *lemma, Context *context) {
    Map *cached_rwr = map_new();
    RewriteResult *rwr = _rewrite(expr, lemma, context, cached_rwr, true);
    map_del(cached_rwr,
            (void *)expr);  // remove it from the map so we don't accidently free the rwr
    map_clear_free_values(cached_rwr);
    return rwr;
}
Expression *rewrite_result_get_original(RewriteResult *result) {
    if (!result) {
        return NULL;
    }
    return result->original;
}

Expression *rewrite_result_get_rewritten(RewriteResult *result) {
    if (!result) {
        return NULL;
    }
    return result->rewritten;
}

DoublyLinkedList *rewrite_result_get_new_goals(RewriteResult *result) {
    if (!result) {
        return NULL;
    }
    return result->new_goals;
}
