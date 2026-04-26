#include <stddef.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

#define CLEAR_CHILD_UPLINK(child_expr, uplink_field)         \
    do {                                                      \
        remove_uplink_by_node((child_expr), (uplink_field)); \
        (uplink_field) = NULL;                                \
    } while (0)

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

static uint64_t g_mark_gen = 0;

static void mark_spine_from(Expression *start, Expression *root, int skip_ownership) {
    size_t cap = 64, front = 0, size = 0;
    Expression **queue = malloc(cap * sizeof(Expression *));
    if (!queue) return;

    start->mark_gen = g_mark_gen;
    queue[size++] = start;

    while (front < size) {
        Expression *node = queue[front++];

        if (node == root) continue;
        if (node->uplinks == NULL) continue;

        DLLNode *ul = node->uplinks->head;
        while (ul != NULL) {
            Uplink *uplink = (Uplink *)ul->data;

            if (!(skip_ownership && is_binder_ownership_edge(uplink->relation))) {
                Expression *parent = (Expression *)uplink->ptr;

                if (parent->mark_gen != g_mark_gen) {
                    parent->mark_gen = g_mark_gen;
                    if (size == cap) {
                        cap *= 2;
                        Expression **ns = realloc(queue, cap * sizeof(Expression *));
                        if (!ns) { free(queue); return; }
                        queue = ns;
                    }
                    queue[size++] = parent;
                }
            }

            ul = ul->next;
        }
    }

    free(queue);
}

static void build_marked_set(Expression *root, DoublyLinkedList *old_exprs) {
    DLLNode *o = old_exprs->head;
    while (o != NULL) {
        mark_spine_from((Expression *)o->data, root, /*skip_ownership=*/0);
        o = o->next;
    }
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
                                 DoublyLinkedList *new_exprs, Map *memo);


static Expression *maybe_rebuild(Context *ctx, Expression *child, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *memo) {
    if (child->mark_gen == g_mark_gen) {
        return spine_rebuild(ctx, child, old_exprs, new_exprs, memo);
    }

    Expression *child_ctx = get_expression_context(child);
    if (child_ctx != NULL && subst_map_lookup(old_exprs, new_exprs, child_ctx) != NULL) {
        child->mark_gen = g_mark_gen;
        return spine_rebuild(ctx, child, old_exprs, new_exprs, memo);
    }

    return child;
}

static Expression *spine_rebuild(Context *apps_ctx, Expression *node, DoublyLinkedList *old_exprs,
                                 DoublyLinkedList *new_exprs, Map *memo) {
    Expression *replacement = subst_map_lookup(old_exprs, new_exprs, node);
    if (replacement != NULL) {
        return replacement;
    }

    if (node->mark_gen != g_mark_gen) {
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
        node->mark_gen = g_mark_gen;
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
            Expression *func2 = maybe_rebuild(apps_ctx, func, old_exprs, new_exprs, memo);
            Expression *arg2 = maybe_rebuild(apps_ctx, arg, old_exprs, new_exprs, memo);

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
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, memo);
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
                mark_spine_from(x_bv, body, /*skip_ownership=*/1);
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 = spine_rebuild(apps_ctx, body, old_exprs, new_exprs, inner_memo);
            pool_map_free(inner_memo);

            if (lambda_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

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
                maybe_rebuild(apps_ctx, x_bv_type, old_exprs, new_exprs, memo);
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
                mark_spine_from(x_bv, body, /*skip_ownership=*/1);
            }

            Map *inner_memo2 = pool_map_alloc();
            Expression *body2 = spine_rebuild(apps_ctx, body, old_exprs, new_exprs, inner_memo2);
            pool_map_free(inner_memo2);

            if (forall_bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            CLEAR_CHILD_UPLINK(x_bv, node->as.forall.bound_variable_uplink_node);
            CLEAR_CHILD_UPLINK(body, node->as.forall.body_uplink_node);

            result = init_forall_expression_wc(x_bv2, body2);
            break;
        }

        case MATCH_EXPRESSION: {
            Expression *scrutinee = node->as.match.scrutinee;
            int branch_cnt = node->as.match.branch_count;

            Expression *scrutinee2 =
                maybe_rebuild(apps_ctx, scrutinee, old_exprs, new_exprs, memo);

            MatchBranch **branches2 = malloc(branch_cnt * sizeof(MatchBranch *));

            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));

                br2->constructor =
                    maybe_rebuild(apps_ctx, br->constructor, old_exprs, new_exprs, memo);
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));

                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_var = br->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);

                    Expression *new_var_type =
                        maybe_rebuild(apps_ctx, old_var_type, old_exprs, new_exprs, memo);
                    Expression *new_var =
                        init_var_expression_wc(get_var_name(old_var), new_var_type, apps_ctx);
                    br2->pattern_variables[j] = new_var;

                    bool match_pv_changed = (new_var != old_var);
                    if (match_pv_changed) {
                        dll_insert_at_tail(old_exprs, dll_new_node(old_var));
                        dll_insert_at_tail(new_exprs, dll_new_node(new_var));
                        mark_spine_from(old_var, br->body, /*skip_ownership=*/1);
                    }
                }

                Map *inner_memo = pool_map_alloc();
                br2->body = spine_rebuild(apps_ctx, br->body, old_exprs, new_exprs, inner_memo);
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
                maybe_rebuild(apps_ctx, rec_var_type, old_exprs, new_exprs, memo);
            Expression *rec_var2 =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type2, apps_ctx);

            Expression **args2 = malloc(arg_count * sizeof(Expression *));

            bool fix_rv_changed = (rec_var2 != rec_var);
            if (fix_rv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
                dll_insert_at_tail(new_exprs, dll_new_node(rec_var2));
                mark_spine_from(rec_var, node->as.fix.body, /*skip_ownership=*/1);
            }

            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = node->as.fix.args[i];
                Expression *old_arg_type = get_expression_type(old_arg);

                Expression *new_arg_type =
                    maybe_rebuild(apps_ctx, old_arg_type, old_exprs, new_exprs, memo);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, apps_ctx);
                args2[i] = new_arg;

                bool fix_arg_changed = (new_arg != old_arg);
                if (fix_arg_changed) {
                    dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                    mark_spine_from(old_arg, node->as.fix.body, /*skip_ownership=*/1);
                }
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 = spine_rebuild(apps_ctx, node->as.fix.body, old_exprs, new_exprs,
                                              inner_memo);
            pool_map_free(inner_memo);

            for (int i = arg_count - 1; i >= 0; i--) {
                if (args2[i] != node->as.fix.args[i]) {
                    free(dll_remove_tail(old_exprs));
                    free(dll_remove_tail(new_exprs));
                }
            }
            if (fix_rv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

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
 * Depth counter: when > 1 we are in a reentrant call.
 * Reentrant calls are dispatched to _simple_topdown_psubst.
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

    ++g_mark_gen;
    if (g_mark_gen == 0) ++g_mark_gen;
    build_marked_set(t, old_exprs);

    if (t->mark_gen != g_mark_gen) {
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
            g_uplink_subst_depth--;
            return t;
        }
        t->mark_gen = g_mark_gen;
    }

    Map *memo = pool_map_alloc();
    Expression *result = spine_rebuild(context, t, old_exprs, new_exprs, memo);
    pool_map_free(memo);

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

    Expression *result = _uplink_p_subst(context, t, old_exprs, new_exprs);

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

    Expression *result = _uplink_p_subst(final_context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    return new_subst(context, t, x, a);
}
