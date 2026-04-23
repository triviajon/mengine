/*
 * subst.c - Uplink-based surgery substitution.
 *
 * Algorithm overview:
 *
 *  Phase 1 – Mark:
 *    Starting from each substitution target (old_exprs entries), follow
 *    uplinks upward through real parent-child usage edges only (never
 *    binder-ownership edges: LAMBDA_BOUND_VAR, FORALL_BOUND_VAR,
 *    FIX_RECURSIVE_VAR, FIX_ARG, MATCH_BRANCH_PATTERN_VAR).
 *    Every node reached on a path to the root t is recorded in a
 *    "marked" set (Map expression* -> sentinel).
 *
 *  Phase 2 – Rebuild (top-down spine walk):
 *    Walk t top-down.  At each node:
 *      - If the node maps to a replacement in the current substitution
 *        map, return the replacement immediately.
 *      - If the node is NOT marked, return it unchanged (shared/untouched).
 *      - If the node IS marked and already memoised at this scope, return
 *        cached result.
 *      - Otherwise path-copy the node, recursing only into marked children.
 *        Unmarked children are reused as-is.  At binder nodes freshen the
 *        bound variable, extend the substitution map, clear the memo, and
 *        continue only along marked children.
 *
 *  Complexity: O(uplink_count * path_length_to_root) for the mark phase
 *  and O(spine_size) for the rebuild phase - proportional to the affected
 *  spine, not the full tree size.
 */

#include <stddef.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

#ifndef TUNE_SUBST_TOPDOWN

/* =========================================================================
 * Map pool: reuse Map objects to avoid repeated malloc/calloc/free overhead.
 * Maps in the pool have been reset (entries cleared) and are ready to use.
 * Pool entries are returned in LIFO order, matching the call stack.
 * ========================================================================= */

#define MAP_POOL_CAPACITY 64
static Map *g_map_pool[MAP_POOL_CAPACITY];
static int g_map_pool_size = 0;

static Map *pool_map_alloc(void) {
    if (g_map_pool_size > 0) {
        return g_map_pool[--g_map_pool_size];
    }
    /* Use a small initial capacity (8) since marked/memo sets are usually tiny. */
    return map_new_with_capacity(8);
}

static void pool_map_free(Map *m) {
    map_reset(m);
    if (g_map_pool_size < MAP_POOL_CAPACITY) {
        g_map_pool[g_map_pool_size++] = m;
    } else {
        map_free(m);
    }
}

/* =========================================================================
 * Helpers: substitution map lookup
 * ========================================================================= */

/*
 * Look up `node` in the parallel substitution lists.
 * Returns the replacement expression, or NULL if not present.
 */
static Expression *subst_map_lookup(DoublyLinkedList *old_exprs, DoublyLinkedList *new_exprs,
                                    Expression *node) {
    DLLNode *o = old_exprs->head;
    DLLNode *n = new_exprs->head;
    while (o != NULL) {
        if ((Expression *)o->data == node) {
            return (Expression *)n->data;
        }
        o = o->next;
        n = n->next;
    }
    return NULL;
}

/* =========================================================================
 * Phase 1: uplink-based spine marking
 * ========================================================================= */

/*
 * Returns non-zero for uplink relations where the child is a binder's own
 * local variable (bound var, recursive var, pattern var, fix arg).  When
 * performing the *secondary* mark_spine_from call inside a binder case of
 * spine_rebuild (to mark body-internal references to the freshened binder
 * variable), we must NOT follow these edges: doing so causes the BFS to
 * escape the binder's scope and mark huge swaths of unrelated expressions
 * (e.g. every APP of `add` when freshening a forall over nat), turning an
 * O(spine) operation into O(n).
 *
 * Phase-1 build_marked_set DOES follow these edges (pass skip_ownership=0)
 * so that binders whose bound-variable types lie on the substitution spine
 * are correctly marked for rebuilding.
 */
static int is_binder_ownership_edge(Relation rel) {
    return rel == LAMBDA_BOUND_VAR || rel == FORALL_BOUND_VAR || rel == FIX_RECURSIVE_VAR ||
           rel == FIX_ARG || rel == MATCH_BRANCH_PATTERN_VAR;
}

/*
 * BFS from `start` upward through usage uplinks, inserting every ancestor
 * node (up to and including `root`) into `marked`.
 *
 * skip_ownership: when non-zero, binder-ownership edges (LAMBDA_BOUND_VAR,
 * FORALL_BOUND_VAR, FIX_RECURSIVE_VAR, FIX_ARG, MATCH_BRANCH_PATTERN_VAR)
 * are NOT followed.  Use this for the secondary re-marking calls inside the
 * binder freshening step so the BFS stays within the binder body and does
 * not escape to the binder's parents (and their entire sub-graphs).
 *
 * `marked` maps Expression* -> (void*)1 (boolean set via pointer sentinel).
 *
 * `root_subtree`: if non-NULL, only follow uplinks to parents that are in
 * this set (i.e., inside the root's subtree). This prevents traversal of
 * accumulated uplinks from previous substitutions that point outside the
 * current root expression.
 */
static void mark_spine_from(Expression *start, Expression *root, Map *marked, int skip_ownership,
                            Map *root_subtree) {
    DoublyLinkedList *queue = dll_create();

    map_set(marked, start, (void *)1);
    dll_insert_at_tail(queue, dll_new_node(start));

    while (queue->head != NULL) {
        DLLNode *front = dll_remove_head(queue);
        Expression *node = (Expression *)front->data;
        free(front);

        if (node == root) {
            /* Reached the root; do not propagate further upward. */
            continue;
        }

        if (node->uplinks == NULL) {
            continue;
        }

        DLLNode *ul = node->uplinks->head;
        while (ul != NULL) {
            Uplink *uplink = (Uplink *)ul->data;

            if (!(skip_ownership && is_binder_ownership_edge(uplink->relation))) {
                Expression *parent = (Expression *)uplink->ptr;

                /* Skip parents outside the root's subtree to avoid following
                 * accumulated uplinks from previous substitutions. */
                if (root_subtree != NULL && map_get(root_subtree, parent) == NULL) {
                    ul = ul->next;
                    continue;
                }

                if (map_get(marked, parent) == NULL) {
                    map_set(marked, parent, (void *)1);
                    dll_insert_at_tail(queue, dll_new_node(parent));
                }
            }

            ul = ul->next;
        }
    }

    dll_destroy(queue);
}

/*
 * Collect all nodes in the subtree rooted at `root` via a downward DFS.
 * Returns a map from Expression* -> (void*)1.
 * The caller is responsible for freeing the map (map_del_all then map_del,
 * or map_clear then map_del).
 */
static void _collect_subtree_dfs(Expression *node, Map *set) {
    if (!node) {
        return;
    }
    if (map_get(set, node)) {
        return; /* Already visited */
    }
    map_set(set, node, (void *)1);

    switch (node->tag) {
        case APP_EXPRESSION:
            _collect_subtree_dfs(node->as.app.func, set);
            _collect_subtree_dfs(node->as.app.arg, set);
            break;
        case LAMBDA_EXPRESSION:
            _collect_subtree_dfs(node->as.lambda.body, set);
            /* Include the bound variable itself so uplinks through it are not
             * filtered out by the root_subtree check in mark_spine_from.
             * Without this, a target on the bv's type path cannot propagate
             * upward through bv to reach the LAMBDA node. */
            _collect_subtree_dfs(node->as.lambda.bound_variable, set);
            _collect_subtree_dfs(get_expression_type(node->as.lambda.bound_variable), set);
            break;
        case FORALL_EXPRESSION:
            _collect_subtree_dfs(node->as.forall.body, set);
            _collect_subtree_dfs(node->as.forall.bound_variable, set);
            _collect_subtree_dfs(get_expression_type(node->as.forall.bound_variable), set);
            break;
        case FIX_EXPRESSION:
            _collect_subtree_dfs(node->as.fix.body, set);
            _collect_subtree_dfs(node->as.fix.recursive_var, set);
            _collect_subtree_dfs(get_expression_type(node->as.fix.recursive_var), set);
            for (int i = 0; i < node->as.fix.arg_count; i++) {
                _collect_subtree_dfs(node->as.fix.args[i], set);
                _collect_subtree_dfs(get_expression_type(node->as.fix.args[i]), set);
            }
            break;
        case MATCH_EXPRESSION:
            _collect_subtree_dfs(node->as.match.scrutinee, set);
            for (int i = 0; i < node->as.match.branch_count; i++) {
                _collect_subtree_dfs(node->as.match.branches[i]->body, set);
                _collect_subtree_dfs(node->as.match.branches[i]->constructor, set);
                for (int j = 0; j < node->as.match.branches[i]->pattern_var_count; j++) {
                    _collect_subtree_dfs(node->as.match.branches[i]->pattern_variables[j], set);
                    _collect_subtree_dfs(
                        get_expression_type(node->as.match.branches[i]->pattern_variables[j]), set);
                }
            }
            break;
        default:
            /* VAR, TYPE, PROP, HOLE: leaf nodes, no children to visit */
            break;
    }
}

/*
 * Global cache: Expression* -> Map* (the cached subtree set).
 * The downward structure of an expression is immutable once created, so the
 * subtree set never changes and can be cached indefinitely.
 * Cached maps are never freed (they live for the program's duration).
 */
static Map *g_subtree_cache = NULL;

static Map *collect_subtree(Expression *root) {
    if (!g_subtree_cache) {
        g_subtree_cache = map_new();
    }
    Map *cached = (Map *)map_get(g_subtree_cache, root);
    if (cached) {
        return cached;
    }
    Map *set = map_new();
    _collect_subtree_dfs(root, set);
    map_set(g_subtree_cache, root, set);
    return set;
}

/*
 * Build and return the marked set: every node that lies on some path from
 * any substitution target to `root`, traced via usage uplinks.
 *
 * Phase 1 follows ALL uplink types (skip_ownership=0) so that binders whose
 * bound-variable types are on the substitution spine get marked too.
 *
 * We first collect the root subtree to filter out accumulated uplinks from
 * previous substitutions that point outside the current expression scope.
 */
/*
 * When `skip_collect_subtree` is true, we do NOT build a root-subtree set to
 * filter accumulated uplinks.  This is safe for beta-reduction: the lambda
 * bound variable being substituted has uplinks only within the lambda body
 * (it is a freshly created private variable), so the uplink BFS naturally
 * stays within the substitution root.  Skipping the O(|subtree|) DFS brings
 * each beta-reduction from O(n) down to O(spine).
 */
static Map *build_marked_set(Expression *root, DoublyLinkedList *old_exprs,
                             bool skip_collect_subtree) {
    Map *marked = pool_map_alloc();
    Map *root_subtree = (int)skip_collect_subtree ? NULL : collect_subtree(root);

    DLLNode *o = old_exprs->head;
    while (o != NULL) {
        Expression *target = (Expression *)o->data;
        mark_spine_from(target, root, marked, /*skip_ownership=*/0, root_subtree);
        o = o->next;
    }

    /* root_subtree is owned by g_subtree_cache; do not free it here. */
    return marked;
}

/* =========================================================================
 * Phase 2: top-down spine rebuild
 * ========================================================================= */

/* Forward declaration. */
static Expression *spine_rebuild(Context *ctx, Expression *node, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo);

/*
 * Rebuild a child if it is marked OR if its context field points to a node
 * being substituted (stale context due to binder freshening).  Otherwise
 * return it unchanged.
 *
 * `ctx` is the context of the current binder scope; it is used when
 * creating new expressions at this level and is passed down recursively
 * so that binder bodies use the freshened binder variable as ctx.
 */
static Expression *maybe_rebuild(Context *ctx, Expression *child, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo) {
    /* Direct substitution target - handled by spine_rebuild step 1. */
    /* Fall through to spine_rebuild if marked. */
    if (map_get(marked, child) != NULL) {
        return spine_rebuild(ctx, child, old_exprs, new_exprs, marked, memo);
    }

    /*
     * Stale-context detection: if child's context field points to a node
     * that is being replaced (e.g. a freshened binder variable), the child
     * must be rebuilt so that the new node receives the translated context.
     * Without this, init_*_expression_wc would reject the child when it is
     * used inside the new binder scope.
     */
    Expression *child_ctx = get_expression_context(child);
    if (child_ctx != NULL && subst_map_lookup(old_exprs, new_exprs, child_ctx) != NULL) {
        map_set(marked, child, (void *)1);
        return spine_rebuild(ctx, child, old_exprs, new_exprs, marked, memo);
    }

    return child;
}

static Expression *spine_rebuild(Context *apps_ctx, Expression *node, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo) {
    /* 1. Substitution map hit - return replacement directly. */
    Expression *replacement = subst_map_lookup(old_exprs, new_exprs, node);
    if (replacement != NULL) {
        return replacement;
    }

    /* 2. Unmarked node - return unchanged unless its context is stale. */
    if (map_get(marked, node) == NULL) {
        Context *nc = get_expression_context(node);
        bool stale = false;
        while (nc != NULL && nc->tag == VAR_EXPRESSION) {
            if (subst_map_lookup(old_exprs, new_exprs, nc) != NULL) {
                stale = true;
                break;
            }
            nc = get_expression_context(nc);
        }
        if (!stale) {
            return node;
        }
        /* Stale context: mark so the switch below rebuilds it. */
        map_set(marked, node, (void *)1);
    }

    /* 3. Memoised result at this scope level. */
    Expression *cached = (Expression *)map_get(memo, node);
    if (cached != NULL) {
        return cached;
    }

    Expression *result = NULL;

    switch (node->tag) {
        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            /* Marked but not in subst map: must be the root in a degenerate
             * case (e.g. root itself is a var that turned out unreachable from
             * targets after careful check).  Return unchanged. */
            result = node;
            break;

        case APP_EXPRESSION: {
            Expression *func = get_app_func(node);
            Expression *arg = get_app_arg(node);
            Expression *func2 = maybe_rebuild(apps_ctx, func, old_exprs, new_exprs, marked, memo);
            Expression *arg2 = maybe_rebuild(apps_ctx, arg, old_exprs, new_exprs, marked, memo);

            if (forms_beta_redex(func2, arg2)) {
                result = beta_reduce(apps_ctx, func2, arg2);
            } else {
                result = init_app_expression_wc(func2, arg2, apps_ctx);
            }
            break;
        }

        case LAMBDA_EXPRESSION: {
            /*
             * fun (x_bv : x_bv_type) => body
             *
             * Freshen the bound variable:
             *   1. Rebuild x_bv_type along the spine.
             *   2. Create x_bv' with apps_ctx (the outermost context, so that
             *      all freshened variables have a context that's stable across
             *      subsequent substitutions).
             *   3. Extend the substitution map: x_bv -> x_bv'.
             *   4. Rebuild body using apps_ctx unchanged (apps_ctx is constant
             *      across binder crossings so rebuilt nodes remain valid in
             *      the caller's scope).
             *   5. Pop the binder extension.
             */
            Expression *x_bv = get_lambda_bound_variable(node);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(node);

            Expression *x_bv_type2 =
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, marked, memo);
            // Reuse the existing bound variable when its type and context are
            // unchanged.  Creating a fresh var unconditionally cascades novel
            // pointers through the rebuilt body, breaking hash-consing sharing
            // for all expressions that reference this binder - turning O(1)
            // substitution into O(body_size) novel expressions per call.
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;  // unchanged: reuse the original bound variable
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }

            /*
             * Only extend the substitution map and re-mark when the bound
             * variable actually changed (hash-consing may return x_bv itself
             * when the type and context are identical).  Inserting a no-op
             * x_bv->x_bv mapping would cause every body node with
             * context==x_bv to trigger the stale-context check, degrading
             * O(spine) to O(body).
             *
             * mark_spine_from with skip_ownership=1 keeps the BFS inside
             * the lambda body; without it the BFS would follow the
             * LAMBDA_BOUND_VAR edge to the binder and then escape upward
             * through the entire expression graph.
             */
            bool lambda_bv_changed = (x_bv2 != x_bv);
            if (lambda_bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
                dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
                mark_spine_from(x_bv, body, marked, /*skip_ownership=*/1, NULL);
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 =
                spine_rebuild(apps_ctx, body, old_exprs, new_exprs, marked, inner_memo);
            pool_map_free(inner_memo);

            if (lambda_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            result = init_lambda_expression_wc(x_bv2, body2);
            break;
        }

        case FORALL_EXPRESSION: {
            /* forall (x_bv : x_bv_type), body - same logic as LAMBDA. */
            Expression *x_bv = get_forall_bound_variable(node);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(node);

            Expression *x_bv_type2 =
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, marked, memo);
            // Same reuse logic as LAMBDA: avoid fresh vars when type/context
            // are unchanged to preserve sharing across substitution calls.
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }

            /* Same no-op and ownership-edge logic as the LAMBDA case above. */
            bool forall_bv_changed = (x_bv2 != x_bv);
            if (forall_bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
                dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
                mark_spine_from(x_bv, body, marked, /*skip_ownership=*/1, NULL);
            }

            Map *inner_memo2 = pool_map_alloc();
            Expression *body2 =
                spine_rebuild(apps_ctx, body, old_exprs, new_exprs, marked, inner_memo2);
            pool_map_free(inner_memo2);

            if (forall_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            result = init_forall_expression_wc(x_bv2, body2);
            break;
        }

        case MATCH_EXPRESSION: {
            Expression *scrutinee = node->as.match.scrutinee;
            int branch_cnt = node->as.match.branch_count;

            Expression *scrutinee2 =
                maybe_rebuild(apps_ctx, scrutinee, old_exprs, new_exprs, marked, memo);

            MatchBranch **branches2 = malloc(branch_cnt * sizeof(MatchBranch *));

            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));

                br2->constructor =
                    maybe_rebuild(apps_ctx, br->constructor, old_exprs, new_exprs, marked, memo);
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));

                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_var = br->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);

                    Expression *new_var_type =
                        maybe_rebuild(apps_ctx, old_var_type, old_exprs, new_exprs, marked, memo);
                    Expression *new_var =
                        init_var_expression_wc(get_var_name(old_var), new_var_type, apps_ctx);
                    br2->pattern_variables[j] = new_var;

                    bool match_pv_changed = (new_var != old_var);
                    if (match_pv_changed) {
                        dll_insert_at_tail(old_exprs, dll_new_node(old_var));
                        dll_insert_at_tail(new_exprs, dll_new_node(new_var));
                        mark_spine_from(old_var, br->body, marked, /*skip_ownership=*/1, NULL);
                    }
                }

                Map *inner_memo = pool_map_alloc();
                br2->body =
                    spine_rebuild(apps_ctx, br->body, old_exprs, new_exprs, marked, inner_memo);
                pool_map_free(inner_memo);

                for (int j = br->pattern_var_count - 1; j >= 0; j--) {
                    Expression *old_var = br->pattern_variables[j];
                    Expression *new_var = br2->pattern_variables[j];
                    if (new_var != old_var) {
                        free(dll_remove_tail(old_exprs));
                        free(dll_remove_tail(new_exprs));
                    }
                }

                branches2[i] = br2;
            }

            result = init_match_expression_wc(scrutinee2, branches2, branch_cnt, apps_ctx);
            break;
        }

        case FIX_EXPRESSION: {
            Expression *rec_var = node->as.fix.recursive_var;
            Expression *rec_var_type = get_expression_type(rec_var);
            int arg_count = node->as.fix.arg_count;

            Expression *rec_var_type2 =
                maybe_rebuild(apps_ctx, rec_var_type, old_exprs, new_exprs, marked, memo);
            Expression *rec_var2 =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type2, apps_ctx);

            Expression **args2 = malloc(arg_count * sizeof(Expression *));

            /* Extend subst map with rec_var -> rec_var2 (if changed). */
            bool fix_rv_changed = (rec_var2 != rec_var);
            if (fix_rv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
                dll_insert_at_tail(new_exprs, dll_new_node(rec_var2));
                mark_spine_from(rec_var, node->as.fix.body, marked, /*skip_ownership=*/1, NULL);
            }

            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = node->as.fix.args[i];
                Expression *old_arg_type = get_expression_type(old_arg);

                Expression *new_arg_type =
                    maybe_rebuild(apps_ctx, old_arg_type, old_exprs, new_exprs, marked, memo);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, apps_ctx);
                args2[i] = new_arg;

                bool fix_arg_changed = (new_arg != old_arg);
                if (fix_arg_changed) {
                    dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                    mark_spine_from(old_arg, node->as.fix.body, marked, /*skip_ownership=*/1, NULL);
                }
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 = spine_rebuild(apps_ctx, node->as.fix.body, old_exprs, new_exprs,
                                              marked, inner_memo);
            pool_map_free(inner_memo);

            /* Pop arg extensions (in reverse order, only if changed). */
            for (int i = arg_count - 1; i >= 0; i--) {
                if (args2[i] != node->as.fix.args[i]) {
                    free(dll_remove_tail(old_exprs));
                    free(dll_remove_tail(new_exprs));
                }
            }
            /* Pop rec_var extension (only if changed). */
            if (fix_rv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            result = init_fix_expression_wc(rec_var2, args2, arg_count,
                                            node->as.fix.decreasing_arg_index, body2);
            break;
        }

        default:
            result = node;
            break;
    }

    /* Cache the result in the current scope-level memo. */
    if (result != NULL && result != node) {
        map_set(memo, node, result);
    }

    return result;
}

/* =========================================================================
 * Core uplink‑based parallel substitution
 * ========================================================================= */

/*
 * _uplink_p_subst - the actual implementation.
 *
 * Runs Phase 1 (mark) then Phase 2 (rebuild) on `t`.
 *
 * `skip_collect_subtree`: when true, skip the O(|subtree|) subtree DFS used
 * to filter external uplinks.  Safe only when all substitution targets are
 * private bound variables with no accumulated uplinks outside `t`.
 */
static Expression *_uplink_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                                   DoublyLinkedList *new_exprs, bool skip_collect_subtree) {
    /* Fast path: t itself is a direct substitution target. */
    Expression *direct = subst_map_lookup(old_exprs, new_exprs, t);
    if (direct != NULL) {
        return direct;
    }

    /* Phase 1: mark the spine. */
    Map *marked = build_marked_set(t, old_exprs, skip_collect_subtree);

    /* If root t was not reached from any target via uplinks, check for
     * stale context: even if the substitution target doesn't appear in
     * t's body, t may have context = bv_old (a substituted binder), and
     * in that case we must rebuild t so the result has the correct context.
     * This mirrors the O(tree) _p_subst check: context_find(t_ctx, old_var). */
    if (map_get(marked, t) == NULL) {
        bool stale = false;
        Context *tc = get_expression_context(t);
        while (tc != NULL && tc->tag == VAR_EXPRESSION) {
            if (subst_map_lookup(old_exprs, new_exprs, tc) != NULL) {
                stale = true;
                break;
            }
            tc = get_expression_context(tc);
        }
        if (!stale) {
            pool_map_free(marked);
            return t;
        }
        /* Mark t so spine_rebuild processes it. */
        map_set(marked, t, (void *)1);
    }

    /* Phase 2: rebuild only the marked spine. */
    Map *memo = pool_map_alloc();
    Expression *result = spine_rebuild(context, t, old_exprs, new_exprs, marked, memo);
    pool_map_free(memo);
    pool_map_free(marked);

    return result;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

/*
 * _p_subst - internal parallel substitution (called by callers that manage
 * context transformations manually, e.g. during recursion in this file).
 */
Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    return _uplink_p_subst(context, t, old_exprs, new_exprs, /*skip_collect_subtree=*/false);
}

/*
 * new_p_subst - public parallel substitution entry point.
 */
Expression *new_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                        DoublyLinkedList *new_exprs) {
    int n = dll_len(old_exprs);
    if (n != dll_len(new_exprs)) {
        return NULL;
    }
    if (n == 0) {
        return t;
    }
    return _uplink_p_subst(context, t, old_exprs, new_exprs, /*skip_collect_subtree=*/false);
}

/*
 * _subst - single-variable substitution called from context_replace and by
 * other callers that manage context transformations manually.
 */
Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) {
        return a;
    }

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();
    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Expression *result =
        _uplink_p_subst(context, t, old_exprs, new_exprs, /*skip_collect_subtree=*/false);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

/*
 * new_subst - public single-variable substitution entry point.
 *
 * Performs the context cut (gamma, x:A, delta -> gamma, delta[x->a]) then
 * runs the uplink-based parallel substitution for x and the delta variables.
 */
Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    Context *final_context = context_cut(context, x, a);

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Context *original_context_c = context;
    Context *final_context_c = final_context;
    while (original_context_c != final_context_c && original_context_c != x) {
        dll_insert_at_head(old_exprs, dll_new_node(original_context_c));
        dll_insert_at_head(new_exprs, dll_new_node(final_context_c));

        original_context_c = get_expression_context(original_context_c);
        final_context_c = get_expression_context(final_context_c);
    }

    Expression *result =
        _uplink_p_subst(final_context, t, old_exprs, new_exprs, /*skip_collect_subtree=*/false);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

/*
 * beta_subst - single-variable substitution for beta-reduction.
 *
 * Like new_subst but skips the O(|subtree|) collect_subtree DFS.  Safe
 * because lambda bound variables are private: their uplinks are confined to
 * the lambda body being substituted into, so the uplink BFS never escapes
 * outside the root.  Extra nodes marked via the LAMBDA_BOUND_VAR uplink are
 * harmless - spine_rebuild only descends from the root and never visits them.
 */
Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    Context *final_context = context_cut(context, x, a);

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Context *original_context_c = context;
    Context *final_context_c = final_context;
    while (original_context_c != final_context_c && original_context_c != x) {
        dll_insert_at_head(old_exprs, dll_new_node(original_context_c));
        dll_insert_at_head(new_exprs, dll_new_node(final_context_c));

        original_context_c = get_expression_context(original_context_c);
        final_context_c = get_expression_context(final_context_c);
    }

    Expression *result =
        _uplink_p_subst(final_context, t, old_exprs, new_exprs, /*skip_collect_subtree=*/true);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

#endif  // TUNE_SUBST_TOPDOWN
