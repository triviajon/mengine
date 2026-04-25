#include <stddef.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

#ifndef TUNE_SUBST_TOPDOWN

#ifdef DISABLE_HASH_CONSING
#define CLEAR_CHILD_UPLINK(child_expr, uplink_field)         \
    do {                                                      \
        remove_uplink_by_node((child_expr), (uplink_field)); \
        (uplink_field) = NULL;                                \
    } while (0)
#else
/*
 * With hash-consing enabled, canonical nodes may be shared by many parents.
 * Clearing child-uplink back-pointers during path-copying can disconnect
 * still-live shared nodes. Keep these links intact in this mode.
 */
#define CLEAR_CHILD_UPLINK(child_expr, uplink_field) \
    do {                                             \
        (void)(child_expr);                          \
        (void)(uplink_field);                        \
    } while (0)
#endif

#define MAP_POOL_CAPACITY 64
static Map *g_map_pool[MAP_POOL_CAPACITY];
static int g_map_pool_size = 0;

static Map *pool_map_alloc(void) {
    if (g_map_pool_size > 0) {
        return g_map_pool[--g_map_pool_size];
    }
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

/*
 * Returns non-zero for uplink relations where the child is a binder's own
 * local variable (bound var, recursive var, pattern var, fix arg).  Used by
 * the secondary mark_spine_from calls inside binder freshening to prevent
 * the BFS from escaping the binder body scope.
 */
static int is_binder_ownership_edge(Relation rel) {
    return rel == LAMBDA_BOUND_VAR || rel == FORALL_BOUND_VAR || rel == FIX_RECURSIVE_VAR ||
           rel == FIX_ARG || rel == MATCH_BRANCH_PATTERN_VAR;
}

/*
 * Generation counter for the subtree stamp.  Each _uplink_p_subst call
 * advances this and stamps root's structural descendants so the mark BFS
 * cannot escape through shared nodes (e.g. a global axiom's bound-variable
 * carries FORALL_BOUND_VAR uplinks that lead to the entire expression graph).
 */
static uint64_t g_subst_gen = 0;

/*
 * DFS from root downward, stamping every structural descendant's visit_gen
 * field with a fresh generation.  Returns the generation used.
 */
static uint64_t stamp_subtree(Expression *root) {
    ++g_subst_gen;
    if (g_subst_gen == 0) ++g_subst_gen;
    uint64_t gen = g_subst_gen;
    if (root == NULL) return gen;

    size_t cap = 64, top = 0;
    Expression **stack = malloc(cap * sizeof(Expression *));
    if (!stack) return gen;

#define SUBST_PUSH(e)                                                          \
    do {                                                                       \
        Expression *_sc = (e);                                                 \
        if (_sc != NULL && _sc->visit_gen != gen) {                            \
            if (top == cap) {                                                  \
                cap *= 2;                                                      \
                Expression **_ns = realloc(stack, cap * sizeof(Expression *)); \
                if (!_ns) { free(stack); return gen; }                         \
                stack = _ns;                                                   \
            }                                                                  \
            stack[top++] = _sc;                                                \
        }                                                                      \
    } while (0)

    SUBST_PUSH(root);
    while (top > 0) {
        Expression *node = stack[--top];
        if (node == NULL || node->visit_gen == gen) continue;
        node->visit_gen = gen;
        switch (node->tag) {
            case APP_EXPRESSION:
                SUBST_PUSH(node->as.app.func);
                SUBST_PUSH(node->as.app.arg);
                break;
            case LAMBDA_EXPRESSION:
                SUBST_PUSH(node->as.lambda.bound_variable);
                /* Also stamp the bound variable's type: substitution targets
                 * that appear in binder types register uplinks there, and the
                 * mark-BFS must reach those nodes via the gen check. */
                SUBST_PUSH(get_expression_type(node->as.lambda.bound_variable));
                SUBST_PUSH(node->as.lambda.body);
                break;
            case FORALL_EXPRESSION:
                SUBST_PUSH(node->as.forall.bound_variable);
                /* Same reasoning as LAMBDA: stamp bound-variable type so BFS
                 * can reach substitution targets appearing in binder types. */
                SUBST_PUSH(get_expression_type(node->as.forall.bound_variable));
                SUBST_PUSH(node->as.forall.body);
                break;
            case FIX_EXPRESSION:
                SUBST_PUSH(node->as.fix.recursive_var);
                SUBST_PUSH(get_expression_type(node->as.fix.recursive_var));
                for (int i = 0; i < node->as.fix.arg_count; i++) {
                    SUBST_PUSH(node->as.fix.args[i]);
                    SUBST_PUSH(get_expression_type(node->as.fix.args[i]));
                }
                SUBST_PUSH(node->as.fix.body);
                break;
            case MATCH_EXPRESSION:
                SUBST_PUSH(node->as.match.scrutinee);
                for (int i = 0; i < node->as.match.branch_count; i++) {
                    MatchBranch *br = node->as.match.branches[i];
                    SUBST_PUSH(br->constructor);
                    for (int j = 0; j < br->pattern_var_count; j++) {
                        SUBST_PUSH(br->pattern_variables[j]);
                        SUBST_PUSH(get_expression_type(br->pattern_variables[j]));
                    }
                    SUBST_PUSH(br->body);
                }
                break;
            default:  /* VAR, HOLE, TYPE, PROP - structural leaves */
                break;
        }
    }
    free(stack);
    return gen;
}
#undef SUBST_PUSH

static void mark_spine_from(Expression *start, Expression *root, Map *marked,
                            int skip_ownership, uint64_t subtree_gen) {
    DoublyLinkedList *queue = dll_create();

    map_set(marked, start, (void *)1);
    dll_insert_at_tail(queue, dll_new_node(start));

    while (queue->head != NULL) {
        DLLNode *front = dll_remove_head(queue);
        Expression *node = (Expression *)front->data;
        free(front);

        if (node == root) {
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

                if (parent->visit_gen == subtree_gen &&
                    map_get(marked, parent) == NULL) {
                    map_set(marked, parent, (void *)1);
                    dll_insert_at_tail(queue, dll_new_node(parent));
                }
            }

            ul = ul->next;
        }
    }

    dll_destroy(queue);
}

static Map *build_marked_set(Expression *root, DoublyLinkedList *old_exprs,
                             uint64_t subtree_gen) {
    Map *marked = pool_map_alloc();

    DLLNode *o = old_exprs->head;
    while (o != NULL) {
        Expression *target = (Expression *)o->data;
        mark_spine_from(target, root, marked, /*skip_ownership=*/0, subtree_gen);
        o = o->next;
    }

    return marked;
}

static Expression *_simple_topdown_psubst(Context *ctx, Expression *t,
                                          DoublyLinkedList *old_exprs,
                                          DoublyLinkedList *new_exprs) {
    Expression *replacement = subst_map_lookup(old_exprs, new_exprs, t);
    if (replacement != NULL) return replacement;

    bool any_in_scope = false;
    Context *tc = get_expression_context(t);
    DLLNode *o = old_exprs->head;
    while (o != NULL) {
        if (context_find(tc, (Expression *)o->data) != NULL) {
            any_in_scope = true;
            break;
        }
        o = o->next;
    }
    if (!any_in_scope) return t;

    switch (t->tag) {
        case APP_EXPRESSION: {
            Expression *func  = get_app_func(t);
            Expression *arg   = get_app_arg(t);
            Expression *func2 = _simple_topdown_psubst(ctx, func, old_exprs, new_exprs);
            Expression *arg2  = _simple_topdown_psubst(ctx, arg,  old_exprs, new_exprs);
            if (func2 == func && arg2 == arg && valid_in_context(t, ctx)) return t;
            if (forms_beta_redex(func2, arg2))
                return beta_reduce(ctx, func2, arg2);
            return init_app_expression_wc(func2, arg2, ctx);
        }

        case LAMBDA_EXPRESSION: {
            Expression *x_bv       = get_lambda_bound_variable(t);
            Expression *x_bv_type  = get_expression_type(x_bv);
            Expression *body       = get_lambda_body(t);
            Expression *x_bv_type2 = _simple_topdown_psubst(ctx, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv2      = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, ctx);
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
            Expression *body2 = _simple_topdown_psubst((Context *)x_bv2, body, old_exprs, new_exprs);
            free(dll_remove_tail(old_exprs));
            free(dll_remove_tail(new_exprs));
            if (x_bv2 == x_bv && body2 == body) return t;
            return init_lambda_expression_wc(x_bv2, body2);
        }

        case FORALL_EXPRESSION: {
            Expression *x_bv       = get_forall_bound_variable(t);
            Expression *x_bv_type  = get_expression_type(x_bv);
            Expression *body       = get_forall_body(t);
            Expression *x_bv_type2 = _simple_topdown_psubst(ctx, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv2      = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, ctx);
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
            Expression *body2 = _simple_topdown_psubst((Context *)x_bv2, body, old_exprs, new_exprs);
            free(dll_remove_tail(old_exprs));
            free(dll_remove_tail(new_exprs));
            if (x_bv2 == x_bv && body2 == body) return t;
            return init_forall_expression_wc(x_bv2, body2);
        }

        default:
            return t;
    }
}

static Expression *spine_rebuild(Context *ctx, Expression *node, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo,
                                 uint64_t subtree_gen);


static Expression *maybe_rebuild(Context *ctx, Expression *child, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo,
                                 uint64_t subtree_gen) {
    if (map_get(marked, child) != NULL) {
        return spine_rebuild(ctx, child, old_exprs, new_exprs, marked, memo, subtree_gen);
    }

    Expression *child_ctx = get_expression_context(child);
    if (child_ctx != NULL && subst_map_lookup(old_exprs, new_exprs, child_ctx) != NULL) {
        map_set(marked, child, (void *)1);
        return spine_rebuild(ctx, child, old_exprs, new_exprs, marked, memo, subtree_gen);
    }

    return child;
}

static Expression *spine_rebuild(Context *apps_ctx, Expression *node, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *marked, Map *memo,
                                 uint64_t subtree_gen) {
    Expression *replacement = subst_map_lookup(old_exprs, new_exprs, node);
    if (replacement != NULL) {
        return replacement;
    }

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
        map_set(marked, node, (void *)1);
    }

    Expression *cached = (Expression *)map_get(memo, node);
    if (cached != NULL) {
        return cached;
    }

    Expression *result = NULL;

    switch (node->tag) {
        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            result = node;
            break;

        case APP_EXPRESSION: {
            Expression *func = get_app_func(node);
            Expression *arg = get_app_arg(node);
            Expression *func2 = maybe_rebuild(apps_ctx, func, old_exprs, new_exprs, marked, memo, subtree_gen);
            Expression *arg2 = maybe_rebuild(apps_ctx, arg, old_exprs, new_exprs, marked, memo, subtree_gen);

            // Remove stale uplinks only in non-hash-consed mode.
            CLEAR_CHILD_UPLINK(func, node->as.app.func_uplink_node);
            CLEAR_CHILD_UPLINK(arg, node->as.app.arg_uplink_node);

            if (forms_beta_redex(func2, arg2)) {
                result = beta_reduce(apps_ctx, func2, arg2);
            } else {
                result = init_app_expression_wc(func2, arg2, apps_ctx);
            }
            break;
        }

        case LAMBDA_EXPRESSION: {
            Expression *x_bv = get_lambda_bound_variable(node);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(node);

            Expression *x_bv_type2 =
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, marked, memo, subtree_gen);
            // Try to reuse the existing bound variable if the type and context are unchanged
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }

            bool lambda_bv_changed = (x_bv2 != x_bv);
            if (lambda_bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
                dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
                mark_spine_from(x_bv, body, marked, /*skip_ownership=*/1, subtree_gen);
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 =
                spine_rebuild(apps_ctx, body, old_exprs, new_exprs, marked, inner_memo, subtree_gen);
            pool_map_free(inner_memo);

            if (lambda_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            // Remove stale uplinks only in non-hash-consed mode.
            CLEAR_CHILD_UPLINK(x_bv, node->as.lambda.bound_variable_uplink_node);
            CLEAR_CHILD_UPLINK(body, node->as.lambda.body_uplink_node);

            result = init_lambda_expression_wc(x_bv2, body2);
            break;
        }

        case FORALL_EXPRESSION: {
            Expression *x_bv = get_forall_bound_variable(node);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(node);

            Expression *x_bv_type2 =
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, marked, memo, subtree_gen);
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }

            bool forall_bv_changed = (x_bv2 != x_bv);
            if (forall_bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
                dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));
                mark_spine_from(x_bv, body, marked, /*skip_ownership=*/1, subtree_gen);
            }

            Map *inner_memo2 = pool_map_alloc();
            Expression *body2 =
                spine_rebuild(apps_ctx, body, old_exprs, new_exprs, marked, inner_memo2, subtree_gen);
            pool_map_free(inner_memo2);

            if (forall_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            // Remove stale uplinks only in non-hash-consed mode.
            CLEAR_CHILD_UPLINK(x_bv, node->as.forall.bound_variable_uplink_node);
            CLEAR_CHILD_UPLINK(body, node->as.forall.body_uplink_node);

            result = init_forall_expression_wc(x_bv2, body2);
            break;
        }

        case MATCH_EXPRESSION: {
            Expression *scrutinee = node->as.match.scrutinee;
            int branch_cnt = node->as.match.branch_count;

            Expression *scrutinee2 =
                maybe_rebuild(apps_ctx, scrutinee, old_exprs, new_exprs, marked, memo, subtree_gen);

            MatchBranch **branches2 = malloc(branch_cnt * sizeof(MatchBranch *));

            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));

                br2->constructor =
                    maybe_rebuild(apps_ctx, br->constructor, old_exprs, new_exprs, marked, memo, subtree_gen);
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));

                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_var = br->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);

                    Expression *new_var_type =
                        maybe_rebuild(apps_ctx, old_var_type, old_exprs, new_exprs, marked, memo, subtree_gen);
                    Expression *new_var =
                        init_var_expression_wc(get_var_name(old_var), new_var_type, apps_ctx);
                    br2->pattern_variables[j] = new_var;

                    bool match_pv_changed = (new_var != old_var);
                    if (match_pv_changed) {
                        dll_insert_at_tail(old_exprs, dll_new_node(old_var));
                        dll_insert_at_tail(new_exprs, dll_new_node(new_var));
                        mark_spine_from(old_var, br->body, marked, /*skip_ownership=*/1, subtree_gen);
                    }
                }

                Map *inner_memo = pool_map_alloc();
                br2->body =
                    spine_rebuild(apps_ctx, br->body, old_exprs, new_exprs, marked, inner_memo, subtree_gen);
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

            // Remove stale uplinks only in non-hash-consed mode.
            CLEAR_CHILD_UPLINK(scrutinee, node->as.match.scrutinee_uplink_node);
            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                CLEAR_CHILD_UPLINK(br->constructor, br->constructor_uplink_node);
                CLEAR_CHILD_UPLINK(br->body, br->body_uplink_node);
                for (int j = 0; j < br->pattern_var_count; j++) {
                    CLEAR_CHILD_UPLINK(br->pattern_variables[j],
                                       br->pattern_variables_uplink_nodes[j]);
                }
            }

            result = init_match_expression_wc(scrutinee2, branches2, branch_cnt, apps_ctx);
            break;
        }

        case FIX_EXPRESSION: {
            Expression *rec_var = node->as.fix.recursive_var;
            Expression *rec_var_type = get_expression_type(rec_var);
            int arg_count = node->as.fix.arg_count;

            Expression *rec_var_type2 =
                maybe_rebuild(apps_ctx, rec_var_type, old_exprs, new_exprs, marked, memo, subtree_gen);
            Expression *rec_var2 =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type2, apps_ctx);

            Expression **args2 = malloc(arg_count * sizeof(Expression *));

            // Extend subst map with rec_var -> rec_var2
            bool fix_rv_changed = (rec_var2 != rec_var);
            if (fix_rv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
                dll_insert_at_tail(new_exprs, dll_new_node(rec_var2));
                mark_spine_from(rec_var, node->as.fix.body, marked, /*skip_ownership=*/1, subtree_gen);
            }

            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = node->as.fix.args[i];
                Expression *old_arg_type = get_expression_type(old_arg);

                Expression *new_arg_type =
                    maybe_rebuild(apps_ctx, old_arg_type, old_exprs, new_exprs, marked, memo, subtree_gen);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, apps_ctx);
                args2[i] = new_arg;

                bool fix_arg_changed = (new_arg != old_arg);
                if (fix_arg_changed) {
                    dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                    mark_spine_from(old_arg, node->as.fix.body, marked, /*skip_ownership=*/1, subtree_gen);
                }
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 = spine_rebuild(apps_ctx, node->as.fix.body, old_exprs, new_exprs,
                                              marked, inner_memo, subtree_gen);
            pool_map_free(inner_memo);

            // Pop arg extensions (in reverse order, only if changed)
            for (int i = arg_count - 1; i >= 0; i--) {
                if (args2[i] != node->as.fix.args[i]) {
                    free(dll_remove_tail(old_exprs));
                    free(dll_remove_tail(new_exprs));
                }
            }
            // Pop rec_var extension (only if changed)
            if (fix_rv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            // Remove stale uplinks only in non-hash-consed mode.
            CLEAR_CHILD_UPLINK(rec_var, node->as.fix.recursive_var_uplink_node);
            for (int i = 0; i < arg_count; i++) {
                CLEAR_CHILD_UPLINK(node->as.fix.args[i], node->as.fix.args_uplink_nodes[i]);
            }
            CLEAR_CHILD_UPLINK(node->as.fix.body, node->as.fix.body_uplink_node);

            result = init_fix_expression_wc(rec_var2, args2, arg_count,
                                            node->as.fix.decreasing_arg_index, body2);
            break;
        }

        default:
            result = node;
            break;
    }

    if (result != NULL && result != node) {
        map_set(memo, node, result);
    }

    return result;
}

/*
 * Depth counter: incremented on each _uplink_p_subst entry, decremented on
 * exit.  When > 1 we are in a reentrant call./ Reentrant
 * calls are dispatched to _simple_topdown_psubst instead of
 * the bottomup algorithm.
 */
static int g_uplink_subst_depth = 0;

static Expression *_simple_topdown_psubst(Context *ctx, Expression *t,
                                          DoublyLinkedList *old_exprs,
                                          DoublyLinkedList *new_exprs);

static Expression *_uplink_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                                   DoublyLinkedList *new_exprs) {
    g_uplink_subst_depth++;

    if (g_uplink_subst_depth > 1) {
        Expression *result = _simple_topdown_psubst(context, t, old_exprs, new_exprs);
        g_uplink_subst_depth--;
        return result;
    }

    Expression *direct = subst_map_lookup(old_exprs, new_exprs, t);
    if (direct != NULL) {
        g_uplink_subst_depth--;
        return direct;
    }

    uint64_t subtree_gen = stamp_subtree(t);

    Map *marked = build_marked_set(t, old_exprs, subtree_gen);

    // If t is not marked, then no substitution targets are reachable from t, so we can skip the spine rebuild entirely and return t as-is.
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
            g_uplink_subst_depth--;
            return t;
        }
        map_set(marked, t, (void *)1);
    }

    // Now, we only need to rebuild along the spines from t to the substitution targets, 
    // so we can use a single memo map for the entire rebuild without worrying about cross-contamination between different branches.
    Map *memo = pool_map_alloc();
    Expression *result = spine_rebuild(context, t, old_exprs, new_exprs, marked, memo, subtree_gen);
    pool_map_free(memo);
    pool_map_free(marked);

    g_uplink_subst_depth--;
    return result;
}

/* =========================================================================
 * Public API
 * ========================================================================= */


Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    return _uplink_p_subst(context, t, old_exprs, new_exprs);
}

Expression *new_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                        DoublyLinkedList *new_exprs) {
    int n = dll_len(old_exprs);
    if (n != dll_len(new_exprs)) {
        return NULL;
    }
    if (n == 0) {
        return t;
    }
    return _uplink_p_subst(context, t, old_exprs, new_exprs);
}

Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) {
        return a;
    }

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();
    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Expression *result =
        _uplink_p_subst(context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

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
        _uplink_p_subst(final_context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    return new_subst(context, t, x, a);
}

#endif  // TUNE_SUBST_TOPDOWN
