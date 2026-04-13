#include "src/engine/rewrite_internal.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/engine/unify.h"
#include "src/runtime/core.h"

/* ============================================================================
 * Rewrite cache debug statistics
 * ============================================================================ */

static bool g_rewrite_debug = false;

typedef struct {
    long hits;             // cache hits in _rewrite
    long misses;           // cache misses in _rewrite
    long mid_result_hits;  // rewrite_head skipped via mid-result cache lookup
    long calls;            // top-level rewrite()/erewrite() invocations
    long total_hits;
    long total_misses;
    long total_mid_result_hits;
    long total_calls;
} RewriteCacheStats;

static RewriteCacheStats g_rwr_stats = {0};

void rewrite_set_debug(bool enabled) { g_rewrite_debug = enabled; }

void rewrite_print_cumulative_stats(void) {
    if (!g_rewrite_debug) return;
    fprintf(stderr,
            "[rewrite cache] cumulative: %ld calls, %ld hits, %ld misses "
            "(hit rate %.1f%%), %ld mid-result hits (rewrite_head skipped)\n",
            g_rwr_stats.total_calls, g_rwr_stats.total_hits, g_rwr_stats.total_misses,
            g_rwr_stats.total_calls > 0
                ? 100.0 * g_rwr_stats.total_hits /
                      (g_rwr_stats.total_hits + g_rwr_stats.total_misses)
                : 0.0,
            g_rwr_stats.total_mid_result_hits);
}

static void _rwr_stats_begin_call(void) {
    g_rwr_stats.hits = 0;
    g_rwr_stats.misses = 0;
    g_rwr_stats.mid_result_hits = 0;
    g_rwr_stats.calls++;
    g_rwr_stats.total_calls++;
}

static void _rwr_stats_print_call(void) {
    if (!g_rewrite_debug) return;
    long total = g_rwr_stats.hits + g_rwr_stats.misses;
    fprintf(stderr,
            "[rewrite cache] call #%ld: %ld hits / %ld lookups (%.1f%%), "
            "%ld mid-result hits\n",
            g_rwr_stats.calls, g_rwr_stats.hits, total,
            total > 0 ? 100.0 * g_rwr_stats.hits / total : 0.0,
            g_rwr_stats.mid_result_hits);
    g_rwr_stats.total_hits += g_rwr_stats.hits;
    g_rwr_stats.total_misses += g_rwr_stats.misses;
    g_rwr_stats.total_mid_result_hits += g_rwr_stats.mid_result_hits;
}

/* ============================================================================
 * Relation Registry
 * ============================================================================ */

#ifndef MAX_RELATIONS
#define MAX_RELATIONS 64
#endif

struct RelationRegistry {
    RelationInfo entries[MAX_RELATIONS];
    int count;
};

RelationRegistry *relation_registry_new(void) {
    RelationRegistry *reg = calloc(1, sizeof(RelationRegistry));
    return reg;
}

void relation_registry_free(RelationRegistry *reg) {
    if (reg) {
        free(reg);
    }
}

bool relation_registry_add(RelationRegistry *reg, RelationInfo info) {
    if (!reg || reg->count >= MAX_RELATIONS) {
        return false;
    }
    /* overwrite if relation already registered */
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].relation == info.relation) {
            reg->entries[i] = info;
            return true;
        }
    }
    reg->entries[reg->count++] = info;
    return true;
}

RelationInfo *relation_registry_lookup(RelationRegistry *reg, Expression *relation) {
    if (!reg) {
        return NULL;
    }
    for (int i = 0; i < reg->count; i++) {
        if (reg->entries[i].relation == relation) {
            return &reg->entries[i];
        }
    }
    return NULL;
}

/* ============================================================================
 * Rewrite internals
 * ============================================================================ */

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

    // Free f_x only if it is a fresh, unshared expression (uplinks == 0).
    // With hash-consing, init_app_expression_wc may return a cached expression
    // that is referenced by other parents (e.g., it IS the expr being rewritten).
    // Unconditionally calling free_expression on a shared expression is unsound.
    if (f_x->uplinks == NULL || f_x->uplink_count == 0) {
        free_expression(f_x);
    }
    // else: f_x is a shared/interned expression; B remains live via the proof's
    // reference and f_x will be freed by the uplink system when its parents are freed.

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
    if (rwr->new_goals) {
        dll_destroy(rwr->new_goals);
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

    if (!allow_unresolved_bindings && unif_result->new_goals->head != NULL) {
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
    unif_result->new_goals = NULL;  // transferred to rewrite result
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
        rwr_func->new_goals = NULL;  // transferred to mid_rwr
        rwr_arg->new_goals = NULL;   // transferred to mid_rwr
    }

    // Optimization: if mid_rwr->rewritten is already in the cache (possible via
    // hash-consing returning the same interned pointer as a previously-visited
    // subterm), its full rewrite result is equivalent to what rewrite_head would
    // produce — because the children of mid_rwr->rewritten are already fully
    // rewritten, so the cached full-rewrite and the bare head-only rewrite coincide.
#ifndef DISABLE_REWRITE_CACHE
    bool mid_result_from_cache = false;
    RewriteResult *mid_result = (RewriteResult *)map_get(cached_rwr, (void *)mid_rwr->rewritten);
    if (mid_result) {
        mid_result_from_cache = true;
        g_rwr_stats.mid_result_hits++;
    } else {
        mid_result = rewrite_head(mid_rwr->rewritten, lemma, context, allow_unresolved_bindings);
    }
#else
    bool mid_result_from_cache = false;
    RewriteResult *mid_result =
        rewrite_head(mid_rwr->rewritten, lemma, context, allow_unresolved_bindings);
#endif

    RewriteResult *final_rwr;
    if (rewrite_is_noop(mid_result)) {
        final_rwr = init_rewrite_result(expr, mid_rwr->rewritten, mid_rwr->new_goals,
                                        mid_rwr->original_to_rewritten_proof);
        mid_rwr->new_goals = NULL;  // transferred to final_rwr
    } else {
        final_rwr = init_rewrite_result(expr, mid_result->rewritten,
                                        dll_merge(mid_rwr->new_goals, mid_result->new_goals),
                                        _build_transitivity_proof(mid_rwr, mid_result, context));
        mid_rwr->new_goals = NULL;  // transferred to final_rwr
        if (!mid_result_from_cache) {
            mid_result->new_goals = NULL;  // transferred to final_rwr; skip if cache-owned
        }
    }

    free_rewrite_result(mid_rwr);
    if (!mid_result_from_cache) {
        free_rewrite_result(mid_result);
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
#ifndef DISABLE_REWRITE_CACHE
    RewriteResult *cached = (RewriteResult *)map_get(cached_rwr, (void *)expr);
    if (cached) {
        g_rwr_stats.hits++;
        return cached;
    }
    g_rwr_stats.misses++;
#endif

    RewriteResult *result;
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

#ifndef DISABLE_REWRITE_CACHE
    map_set(cached_rwr, (void *)expr, result);
#endif

    return result;
}

RewriteResult *rewrite(Expression *expr, Expression *lemma, Context *context) {
    _rwr_stats_begin_call();
    Map *cached_rwr = map_new();
    RewriteResult *rwr = _rewrite(expr, lemma, context, cached_rwr, false);
    map_del(cached_rwr,
            (void *)expr);  // remove it from the map so we don't accidently free the rwr
    map_clear_apply_free(cached_rwr, (void (*)(void *))free_rewrite_result);
    _rwr_stats_print_call();
    return rwr;
}

RewriteResult *erewrite(Expression *expr, Expression *lemma, Context *context) {
    _rwr_stats_begin_call();
    Map *cached_rwr = map_new();
    RewriteResult *rwr = _rewrite(expr, lemma, context, cached_rwr, true);
    map_del(cached_rwr,
            (void *)expr);  // remove it from the map so we don't accidently free the rwr
    map_clear_apply_free(cached_rwr, (void (*)(void *))free_rewrite_result);
    _rwr_stats_print_call();
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
