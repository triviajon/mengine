#include <stddef.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

#ifndef MENGINE_SUBST_MEMO
#define MENGINE_SUBST_MEMO 1
#endif

#define CLEAR_CHILD_UPLINK(child_expr, uplink_field)         \
    do {                                                     \
        remove_uplink_by_node((child_expr), (uplink_field)); \
        (uplink_field) = NULL;                               \
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

static int is_binder_ownership_edge(Relation rel) {
    return rel == LAMBDA_BOUND_VAR || rel == FORALL_BOUND_VAR || rel == FIX_RECURSIVE_VAR ||
           rel == FIX_ARG || rel == MATCH_BRANCH_PATTERN_VAR;
}

static uint64_t g_mark_gen = 0;
static uint64_t g_subst_gen = 0;

#define STAMP_PUSH(e)                                                                \
    do {                                                                             \
        Expression *_sc = (e);                                                       \
        if (_sc != NULL && _sc->visit_gen != gen) {                                  \
            if (st_top == st_cap) {                                                  \
                st_cap *= 2;                                                         \
                Expression **_ns = realloc(st_stack, st_cap * sizeof(Expression *)); \
                if (!_ns) {                                                          \
                    free(st_stack);                                                  \
                    return gen;                                                      \
                }                                                                    \
                st_stack = _ns;                                                      \
            }                                                                        \
            st_stack[st_top++] = _sc;                                                \
        }                                                                            \
    } while (0)

static uint64_t stamp_subtree(Expression *root) {
    ++g_subst_gen;
    if (g_subst_gen == 0) {
        ++g_subst_gen;
    }
    uint64_t gen = g_subst_gen;
    if (root == NULL) {
        return gen;
    }

    size_t st_cap = 64;
    size_t st_top = 0;
    Expression **st_stack = malloc(st_cap * sizeof(Expression *));
    if (!st_stack) {
        return gen;
    }

    STAMP_PUSH(root);
    while (st_top > 0) {
        Expression *node = st_stack[--st_top];
        if (node == NULL || node->visit_gen == gen) {
            continue;
        }
        node->visit_gen = gen;
        switch (node->tag) {
            case APP_EXPRESSION:
                STAMP_PUSH(node->as.app.func);
                STAMP_PUSH(node->as.app.arg);
                break;
            case LAMBDA_EXPRESSION:
                STAMP_PUSH(node->as.lambda.bound_variable);
                STAMP_PUSH(get_expression_type(node->as.lambda.bound_variable));
                STAMP_PUSH(node->as.lambda.body);
                break;
            case FORALL_EXPRESSION:
                STAMP_PUSH(node->as.forall.bound_variable);
                STAMP_PUSH(get_expression_type(node->as.forall.bound_variable));
                STAMP_PUSH(node->as.forall.body);
                break;
            case FIX_EXPRESSION:
                STAMP_PUSH(node->as.fix.recursive_var);
                STAMP_PUSH(get_expression_type(node->as.fix.recursive_var));
                for (int i = 0; i < node->as.fix.arg_count; i++) {
                    STAMP_PUSH(node->as.fix.args[i]);
                    STAMP_PUSH(get_expression_type(node->as.fix.args[i]));
                }
                STAMP_PUSH(node->as.fix.body);
                break;
            case MATCH_EXPRESSION:
                STAMP_PUSH(node->as.match.scrutinee);
                for (int i = 0; i < node->as.match.branch_count; i++) {
                    MatchBranch *br = node->as.match.branches[i];
                    STAMP_PUSH(br->constructor);
                    for (int j = 0; j < br->pattern_var_count; j++) {
                        STAMP_PUSH(br->pattern_variables[j]);
                        STAMP_PUSH(get_expression_type(br->pattern_variables[j]));
                    }
                    STAMP_PUSH(br->body);
                }
                break;
            default:
                break;
        }
    }
    free(st_stack);
    return gen;
}
#undef STAMP_PUSH

static bool mark_spine_from(Expression *start, Expression *root, int skip_ownership,
                            uint64_t subtree_gen) {
    size_t cap = 64;
    size_t front = 0;
    size_t size = 0;
    Expression **queue = malloc(cap * sizeof(Expression *));
    if (!queue) {
        return false;
    }
    bool reached_root = (start == root);

    start->mark_gen = g_mark_gen;
    queue[size++] = start;

    while (front < size) {
        Expression *node = queue[front++];
        if (node == root) {
            continue;
        }
        if (!node->uplinks) {
            continue;
        }

        DLLNode *ul = node->uplinks->head;
        while (ul) {
            Uplink *uplink = (Uplink *)ul->data;
            if (!(skip_ownership && is_binder_ownership_edge(uplink->relation))) {
                Expression *parent = (Expression *)uplink->ptr;
                if (parent == root) {
                    reached_root = true;
                }
                if (parent->visit_gen == subtree_gen && parent->mark_gen != g_mark_gen) {
                    parent->mark_gen = g_mark_gen;
                    if (size == cap) {
                        cap *= 2;
                        Expression **ns = realloc(queue, cap * sizeof(Expression *));
                        if (!ns) {
                            free(queue);
                            return reached_root;
                        }
                        queue = ns;
                    }
                    queue[size++] = parent;
                }
            }
            ul = ul->next;
        }
    }
    free(queue);
    return reached_root;
}

// Callbacks for map_for_each -------------------------------------------------

struct mark_args {
    Expression *root;
    uint64_t subtree_gen;
};
static void mark_target(void *key, void *value, void *ud) {
    (void)value;
    struct mark_args *a = (struct mark_args *)ud;
    mark_spine_from((Expression *)key, a->root, 0, a->subtree_gen);
}

static bool subst_map_touches_context(Context *ctx, Map *subst_map) {
    while (ctx && ctx->tag == VAR_EXPRESSION) {
        if (map_get(subst_map, ctx)) {
            return true;
        }
        ctx = get_expression_context(ctx);
    }
    return false;
}

static Expression *subst_replacement_for(Map *subst_map, Expression *expr) {
    if (expr->tag != VAR_EXPRESSION && expr->tag != HOLE_EXPRESSION) {
        return NULL;
    }
    return map_get(subst_map, expr);
}

static bool has_non_binder_uplink(Expression *expr) {
    if (!expr || !expr->uplinks) {
        return false;
    }

    DLLNode *node = expr->uplinks->head;
    while (node) {
        Uplink *uplink = (Uplink *)node->data;
        if (!is_binder_ownership_edge(uplink->relation)) {
            return true;
        }
        node = node->next;
    }
    return false;
}

static bool substitution_skip_by_uplinks(Expression *t, Expression *x) {
    if (!t || !x || t == x || has_non_binder_uplink(x)) {
        return false;
    }

    // Uplinks do not include context pointers; returning t is only valid if
    // t's own minimal context does not need the x-binding cut out.
    Context *term_context = get_expression_context(t);
    return context_find(term_context, x) == NULL;
}

// ----------------------------------------------------------------------------

static void build_marked_set(Expression *root, Map *subst_map, uint64_t subtree_gen) {
    struct mark_args a = {root, subtree_gen};
    map_for_each(subst_map, mark_target, &a);
}

static Expression *_simple_topdown_psubst(Context *ctx, Expression *t, Map *subst_map) {
    Expression *replacement = subst_replacement_for(subst_map, t);
    if (replacement) {
        return replacement;
    }

    if (!subst_map_touches_context(get_expression_context(t), subst_map)) {
        return t;
    }

    switch (t->tag) {
        case APP_EXPRESSION: {
            Expression *func = get_app_func(t);
            Expression *arg = get_app_arg(t);
            Expression *func2 = _simple_topdown_psubst(ctx, func, subst_map);
            Expression *arg2 = _simple_topdown_psubst(ctx, arg, subst_map);
            if (func2 == func && arg2 == arg && valid_in_context(t, ctx)) {
                return t;
            }
            if (forms_beta_redex(func2, arg2) && func->tag != VAR_EXPRESSION) {
                return beta_reduce(ctx, func2, arg2);
            }
            return init_app_expression_wc(func2, arg2, ctx);
        }
        case LAMBDA_EXPRESSION: {
            Expression *x_bv = get_lambda_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(t);
            Expression *x_bv_type2 = _simple_topdown_psubst(ctx, x_bv_type, subst_map);
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, ctx);
            }
            map_set(subst_map, x_bv, x_bv2);
            Expression *body2 = _simple_topdown_psubst((Context *)x_bv2, body, subst_map);
            map_del(subst_map, x_bv);
            if (x_bv2 == x_bv && body2 == body) {
                return t;
            }
            return init_lambda_expression_wc(x_bv2, body2);
        }
        case FORALL_EXPRESSION: {
            Expression *x_bv = get_forall_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(t);
            Expression *x_bv_type2 = _simple_topdown_psubst(ctx, x_bv_type, subst_map);
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, ctx);
            }
            map_set(subst_map, x_bv, x_bv2);
            Expression *body2 = _simple_topdown_psubst((Context *)x_bv2, body, subst_map);
            map_del(subst_map, x_bv);
            if (x_bv2 == x_bv && body2 == body) {
                return t;
            }
            return init_forall_expression_wc(x_bv2, body2);
        }
        case MATCH_EXPRESSION: {
            Expression *scrutinee = get_match_scrutinee(t);
            Expression *scrutinee2 = _simple_topdown_psubst(ctx, scrutinee, subst_map);
            int branch_count = t->as.match.branch_count;
            MatchBranch **branches2 = malloc(branch_count * sizeof(MatchBranch *));
            if (!branches2) {
                return NULL;
            }

            bool changed = scrutinee2 != scrutinee;
            for (int i = 0; i < branch_count; i++) {
                MatchBranch *branch = t->as.match.branches[i];
                MatchBranch *branch2 = calloc(1, sizeof(MatchBranch));
                if (!branch2) {
                    return NULL;
                }

                branch2->constructor = _simple_topdown_psubst(ctx, branch->constructor, subst_map);
                branch2->pattern_var_count = branch->pattern_var_count;
                branch2->pattern_variables =
                    branch->pattern_var_count > 0
                        ? malloc(branch->pattern_var_count * sizeof(Expression *))
                        : NULL;

                Context *branch_ctx = ctx;
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *old_var = branch->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);
                    Expression *new_var_type =
                        _simple_topdown_psubst(branch_ctx, old_var_type, subst_map);
                    Expression *new_var;
                    if (new_var_type == old_var_type &&
                        branch_ctx == get_expression_context(old_var)) {
                        new_var = old_var;
                    } else {
                        new_var =
                            init_var_expression_wc(get_var_name(old_var), new_var_type, branch_ctx);
                    }
                    branch2->pattern_variables[j] = new_var;
                    map_set(subst_map, old_var, new_var);
                    branch_ctx = (Context *)new_var;
                    changed = changed || new_var != old_var;
                }

                branch2->body = _simple_topdown_psubst(branch_ctx, branch->body, subst_map);
                for (int j = branch->pattern_var_count - 1; j >= 0; j--) {
                    map_del(subst_map, branch->pattern_variables[j]);
                }

                changed = changed || branch2->constructor != branch->constructor ||
                          branch2->body != branch->body;
                branches2[i] = branch2;
            }

            if (!changed && valid_in_context(t, ctx)) {
                for (int i = 0; i < branch_count; i++) {
                    free(branches2[i]->pattern_variables);
                    free(branches2[i]);
                }
                free(branches2);
                return t;
            }
            return init_match_expression_wc(scrutinee2, branches2, branch_count, ctx);
        }
        case FIX_EXPRESSION: {
            Expression *rec_var = get_fix_recursive_var(t);
            Expression *rec_var_type = get_expression_type(rec_var);
            Expression *rec_var_type2 = _simple_topdown_psubst(ctx, rec_var_type, subst_map);
            Expression *rec_var2;
            if (rec_var_type2 == rec_var_type && ctx == get_expression_context(rec_var)) {
                rec_var2 = rec_var;
            } else {
                rec_var2 = init_var_expression_wc(get_var_name(rec_var), rec_var_type2, ctx);
            }

            map_set(subst_map, rec_var, rec_var2);
            Context *body_ctx = (Context *)rec_var2;
            int arg_count = get_fix_arg_count(t);
            Expression **args2 = arg_count > 0 ? malloc(arg_count * sizeof(Expression *)) : NULL;
            if (arg_count > 0 && !args2) {
                return NULL;
            }

            bool changed = rec_var2 != rec_var;
            Expression **args = get_fix_args(t);
            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = args[i];
                Expression *old_arg_type = get_expression_type(old_arg);
                Expression *new_arg_type =
                    _simple_topdown_psubst(body_ctx, old_arg_type, subst_map);
                Expression *new_arg;
                if (new_arg_type == old_arg_type && body_ctx == get_expression_context(old_arg)) {
                    new_arg = old_arg;
                } else {
                    new_arg = init_var_expression_wc(get_var_name(old_arg), new_arg_type, body_ctx);
                }
                args2[i] = new_arg;
                map_set(subst_map, old_arg, new_arg);
                body_ctx = (Context *)new_arg;
                changed = changed || new_arg != old_arg;
            }

            Expression *body = get_fix_body(t);
            Expression *body2 = _simple_topdown_psubst(body_ctx, body, subst_map);
            for (int i = arg_count - 1; i >= 0; i--) {
                map_del(subst_map, args[i]);
            }
            map_del(subst_map, rec_var);

            if (!changed && body2 == body && valid_in_context(t, ctx)) {
                free(args2);
                return t;
            }
            return init_fix_expression_wc(rec_var2, args2, arg_count,
                                          get_fix_decreasing_arg_index(t), body2);
        }
        default:
            return t;
    }
}

static Expression *spine_rebuild(Context *ctx, Expression *node, Map *subst_map, Map *memo,
                                 uint64_t subtree_gen);

static Expression *maybe_rebuild(Context *ctx, Expression *child, Map *subst_map, Map *memo,
                                 uint64_t subtree_gen) {
    if (child->mark_gen == g_mark_gen) {
        return spine_rebuild(ctx, child, subst_map, memo, subtree_gen);
    }
    Expression *child_ctx = get_expression_context(child);
    if (child_ctx && map_get(subst_map, child_ctx)) {
        child->mark_gen = g_mark_gen;
        return spine_rebuild(ctx, child, subst_map, memo, subtree_gen);
    }
    return child;
}

static Expression *spine_rebuild(Context *apps_ctx, Expression *node, Map *subst_map, Map *memo,
                                 uint64_t subtree_gen) {
    Expression *replacement = subst_replacement_for(subst_map, node);
    if (replacement) {
        return replacement;
    }

    if (node->mark_gen != g_mark_gen) {
        Context *nc = get_expression_context(node);
        bool stale = false;
        while (nc && nc->tag == VAR_EXPRESSION) {
            if (map_get(subst_map, nc)) {
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

    (void)memo;
#if MENGINE_SUBST_MEMO
    Expression *cached = map_get(memo, node);
    if (cached) {
        return cached;
    }
#endif

    Expression *result = NULL;

    switch (node->tag) {
        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            result = node;
            break;

        case APP_EXPRESSION: {
            Expression *func = get_app_func(node);
            Expression *arg = get_app_arg(node);
            Expression *func2 = maybe_rebuild(apps_ctx, func, subst_map, memo, subtree_gen);
            Expression *arg2 = maybe_rebuild(apps_ctx, arg, subst_map, memo, subtree_gen);
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
                maybe_rebuild(apps_ctx, x_bv_type, subst_map, memo, subtree_gen);
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }
            bool bv_changed = (x_bv2 != x_bv);
            bool body_mentions_bv = false;
            if (bv_changed) {
                map_set(subst_map, x_bv, x_bv2);
                body_mentions_bv = mark_spine_from(x_bv, body, 1, subtree_gen);
            }
            Map *inner_memo = pool_map_alloc();
            Context *body_ctx = (bv_changed && body_mentions_bv) ? (Context *)x_bv2 : apps_ctx;
            Expression *body2 = spine_rebuild(body_ctx, body, subst_map, inner_memo, subtree_gen);
            pool_map_free(inner_memo);
            if (bv_changed) {
                map_del(subst_map, x_bv);
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
                maybe_rebuild(apps_ctx, x_bv_type, subst_map, memo, subtree_gen);
            Expression *x_bv2;
            if (x_bv_type2 == x_bv_type && apps_ctx == get_expression_context(x_bv)) {
                x_bv2 = x_bv;
            } else {
                x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, apps_ctx);
            }
            bool bv_changed = (x_bv2 != x_bv);
            bool body_mentions_bv = false;
            if (bv_changed) {
                map_set(subst_map, x_bv, x_bv2);
                body_mentions_bv = mark_spine_from(x_bv, body, 1, subtree_gen);
            }
            Map *inner_memo = pool_map_alloc();
            Context *body_ctx = (bv_changed && body_mentions_bv) ? (Context *)x_bv2 : apps_ctx;
            Expression *body2 = spine_rebuild(body_ctx, body, subst_map, inner_memo, subtree_gen);
            pool_map_free(inner_memo);
            if (bv_changed) {
                map_del(subst_map, x_bv);
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
                maybe_rebuild(apps_ctx, scrutinee, subst_map, memo, subtree_gen);
            MatchBranch **branches2 = malloc(branch_cnt * sizeof(MatchBranch *));

            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));
                br2->constructor =
                    maybe_rebuild(apps_ctx, br->constructor, subst_map, memo, subtree_gen);
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));

                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_var = br->pattern_variables[j];
                    Expression *new_var_type = maybe_rebuild(apps_ctx, get_expression_type(old_var),
                                                             subst_map, memo, subtree_gen);
                    Expression *new_var =
                        init_var_expression_wc(get_var_name(old_var), new_var_type, apps_ctx);
                    br2->pattern_variables[j] = new_var;
                    if (new_var != old_var) {
                        map_set(subst_map, old_var, new_var);
                        mark_spine_from(old_var, br->body, 1, subtree_gen);
                    }
                }

                Map *inner_memo = pool_map_alloc();
                br2->body = spine_rebuild(apps_ctx, br->body, subst_map, inner_memo, subtree_gen);
                pool_map_free(inner_memo);

                for (int j = br->pattern_var_count - 1; j >= 0; j--) {
                    if (br2->pattern_variables[j] != br->pattern_variables[j]) {
                        map_del(subst_map, br->pattern_variables[j]);
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
                maybe_rebuild(apps_ctx, rec_var_type, subst_map, memo, subtree_gen);
            Expression *rec_var2 =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type2, apps_ctx);
            Expression **args2 = malloc(arg_count * sizeof(Expression *));

            bool fix_rv_changed = (rec_var2 != rec_var);
            if (fix_rv_changed) {
                map_set(subst_map, rec_var, rec_var2);
                mark_spine_from(rec_var, node->as.fix.body, 1, subtree_gen);
            }

            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = node->as.fix.args[i];
                Expression *new_arg_type = maybe_rebuild(apps_ctx, get_expression_type(old_arg),
                                                         subst_map, memo, subtree_gen);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, apps_ctx);
                args2[i] = new_arg;
                if (new_arg != old_arg) {
                    map_set(subst_map, old_arg, new_arg);
                    mark_spine_from(old_arg, node->as.fix.body, 1, subtree_gen);
                }
            }

            Map *inner_memo = pool_map_alloc();
            Expression *body2 =
                spine_rebuild(apps_ctx, node->as.fix.body, subst_map, inner_memo, subtree_gen);
            pool_map_free(inner_memo);

            for (int i = arg_count - 1; i >= 0; i--) {
                if (args2[i] != node->as.fix.args[i]) {
                    map_del(subst_map, node->as.fix.args[i]);
                }
            }
            if (fix_rv_changed) {
                map_del(subst_map, rec_var);
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

    if (result && result != node) {
#if MENGINE_SUBST_MEMO
        map_set(memo, node, result);
#endif
    }
    return result;
}

/*
 * Depth counter: when > 1 we are in a reentrant call (from type construction
 * during spine_rebuild).  Reentrant calls use _simple_topdown_psubst.
 */
static int g_uplink_subst_depth = 0;

__attribute__((unused)) static Expression *_uplink_p_subst(Context *context, Expression *t,
                                                           Map *subst_map) {
    g_uplink_subst_depth++;

    if (g_uplink_subst_depth > 1) {
        Expression *result = _simple_topdown_psubst(context, t, subst_map);
        g_uplink_subst_depth--;
        return result;
    }

    Expression *direct = subst_replacement_for(subst_map, t);
    if (direct) {
        g_uplink_subst_depth--;
        return direct;
    }

    ++g_mark_gen;
    if (g_mark_gen == 0) {
        ++g_mark_gen;
    }
    uint64_t subtree_gen = stamp_subtree(t);
    build_marked_set(t, subst_map, subtree_gen);

    if (t->mark_gen != g_mark_gen) {
        bool stale = false;
        Context *tc = get_expression_context(t);
        while (tc && tc->tag == VAR_EXPRESSION) {
            if (map_get(subst_map, tc)) {
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
    Expression *result = spine_rebuild(context, t, subst_map, memo, subtree_gen);
    pool_map_free(memo);

    g_uplink_subst_depth--;
    return result;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    Map *subst_map = pool_map_alloc();
    DLLNode *o = old_exprs->head;
    DLLNode *n = new_exprs->head;
    while (o) {
        map_set(subst_map, o->data, n->data);
        o = o->next;
        n = n->next;
    }
    Expression *result = _simple_topdown_psubst(context, t, subst_map);
    pool_map_free(subst_map);
    return result;
}

Expression *new_p_subst(Context *context, Expression *t, Map *subst_map) {
    if (!subst_map) {
        return t;
    }
    return _simple_topdown_psubst(context, t, subst_map);
}

Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) {
        return a;
    }
    Map *subst_map = pool_map_alloc();
    map_set(subst_map, x, a);
    Expression *result = _simple_topdown_psubst(context, t, subst_map);
    pool_map_free(subst_map);
    return result;
}

Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (substitution_skip_by_uplinks(t, x)) {
        return t;
    }

    Context *final_context = context_cut(context, x, a);
    Map *subst_map = pool_map_alloc();
    map_set(subst_map, x, a);
    Context *oc = context;
    Context *fc = final_context;
    while (oc != fc && oc != x) {
        map_set(subst_map, oc, fc);
        oc = get_expression_context(oc);
        fc = get_expression_context(fc);
    }
    Expression *result = _simple_topdown_psubst(final_context, t, subst_map);
    pool_map_free(subst_map);
    return result;
}

Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    return new_subst(context, t, x, a);
}
