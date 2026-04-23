
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/expression_hash.h"
#include "src/kernel/subst.h"

#ifdef TUNE_SUBST_TOPDOWN

/* =========================================================================
 * Substitution map: a parallel list of (old -> new) pairs.
 * We push/pop by manipulating the DLL tails directly.
 * ========================================================================= */

static Expression *subst_map_lookup(DoublyLinkedList *old_exprs, DoublyLinkedList *new_exprs,
                                    Expression *node) {
    DLLNode *o = old_exprs->head;
    DLLNode *n = new_exprs->head;
    while (o != NULL) {
        if ((Expression *)o->data == node) return (Expression *)n->data;
        o = o->next;
        n = n->next;
    }
    return NULL;
}

/* Compute the minimum ctx_size across all entries in old_exprs. */
static int subst_map_min_depth(DoublyLinkedList *old_exprs) {
    int min = INT_MAX;
    DLLNode *n = old_exprs->head;
    while (n != NULL) {
        int d = ((Expression *)n->data)->ctx_size;
        if (d < min) min = d;
        n = n->next;
    }
    return min == INT_MAX ? 0 : min;
}

/* =========================================================================
 * Core recursive substitution
 * ========================================================================= */

/* min_depth: minimum ctx_size across all current substitution targets.
 * If node->ctx_size < min_depth, no target is in scope in this subtree. */
static Expression *_td_p_subst(Context *ctx, Expression *node, DoublyLinkedList *old_exprs,
                               DoublyLinkedList *new_exprs, int min_depth);

static Expression *_td_p_subst(Context *ctx, Expression *node, DoublyLinkedList *old_exprs,
                               DoublyLinkedList *new_exprs, int min_depth) {
    if (node == NULL) return NULL;

    /* 1. Depth early exit: no target is in scope in this subtree. */
    if (node->ctx_size < min_depth) return node;

    /* 2. Substitution target? */
    Expression *repl = subst_map_lookup(old_exprs, new_exprs, node);
    if (repl != NULL) return repl;

    switch (node->tag) {
        case APP_EXPRESSION: {
            Expression *func = node->as.app.func;
            Expression *arg = node->as.app.arg;
            Expression *func2 = _td_p_subst(ctx, func, old_exprs, new_exprs, min_depth);
            Expression *arg2 = _td_p_subst(ctx, arg, old_exprs, new_exprs, min_depth);
            if (func2 == NULL || arg2 == NULL) return NULL;

            /* Fast-path: if the canonical (minimum-valid) context version of
             * APP(func2, arg2) already exists in the intern table, reuse it.
             * This restores structural sharing lost when new_subst creates APP
             * nodes with inner-binder contexts instead of the shallowest valid
             * context.  The canonical entry (built with a shallower context)
             * has a smaller ctx_size, which lets the early-exit fire correctly
             * for future new_subst calls. */
#ifndef DISABLE_HASH_CONSING
            {
                Expression probe = {0};
                probe.tag = APP_EXPRESSION;
                probe.context = ctx;
                probe.as.app.func = func2;
                probe.as.app.arg = arg2;
                Expression *cached = expression_intern_lookup(&probe);
                if (cached != NULL) return cached;
            }
#endif

            Context *app_ctx = ctx;
            if (!valid_in_context(func2, app_ctx) || !valid_in_context(arg2, app_ctx)) {
                Context *node_ctx = (Context *)get_expression_context(node);
                Expression *mapped_node_ctx_expr =
                    subst_map_lookup(old_exprs, new_exprs, (Expression *)node_ctx);
                Context *mapped_node_ctx = (Context *)mapped_node_ctx_expr;

                if (mapped_node_ctx != NULL && valid_in_context(func2, mapped_node_ctx) &&
                    valid_in_context(arg2, mapped_node_ctx)) {
                    app_ctx = mapped_node_ctx;
                } else if (valid_in_context(func2, node_ctx) && valid_in_context(arg2, node_ctx)) {
                    app_ctx = node_ctx;
                } else {
                    Context *arg_ctx = (Context *)get_expression_context(arg2);
                    Context *func_ctx = (Context *)get_expression_context(func2);
                    if (valid_in_context(func2, arg_ctx) && valid_in_context(arg2, arg_ctx)) {
                        app_ctx = arg_ctx;
                    } else if (valid_in_context(func2, func_ctx) &&
                               valid_in_context(arg2, func_ctx)) {
                        app_ctx = func_ctx;
                    } else {
                        return NULL;
                    }
                }
            }

            if (func2 == func && arg2 == arg &&
                app_ctx == (Context *)get_expression_context(node)) {
                return node;
            }
            if (forms_beta_redex(func2, arg2)) return beta_reduce(app_ctx, func2, arg2);
            return init_app_expression_wc(func2, arg2, app_ctx);
        }

        case LAMBDA_EXPRESSION: {
            Expression *bv = node->as.lambda.bound_variable;
            Expression *bv_type = get_expression_type(bv);
            Expression *body = node->as.lambda.body;

            Expression *bv_type2 = _td_p_subst(ctx, bv_type, old_exprs, new_exprs, min_depth);
            if (bv_type2 == NULL) return NULL;

            Expression *bv2;
            bool bv_changed;
            if (bv_type2 == bv_type && ctx == (Context *)get_expression_context(bv)) {
                bv2 = bv;
                bv_changed = false;
            } else {
                bv2 = init_var_expression_wc(get_var_name(bv), bv_type2, ctx);
                bv_changed = (bv2 != bv);
            }

            int body_min = min_depth;
            if (bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(bv));
                dll_insert_at_tail(new_exprs, dll_new_node(bv2));
                if (bv->ctx_size < body_min) body_min = bv->ctx_size;
            }
            Expression *body2 = _td_p_subst((Context *)bv2, body, old_exprs, new_exprs, body_min);
            if (bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            if (body2 == NULL) return NULL;

            if (!bv_changed && body2 == body) return node;
            return init_lambda_expression_wc(bv2, body2);
        }

        case FORALL_EXPRESSION: {
            Expression *bv = node->as.forall.bound_variable;
            Expression *bv_type = get_expression_type(bv);
            Expression *body = node->as.forall.body;

            Expression *bv_type2 = _td_p_subst(ctx, bv_type, old_exprs, new_exprs, min_depth);
            if (bv_type2 == NULL) return NULL;

            Expression *bv2;
            bool bv_changed;
            if (bv_type2 == bv_type && ctx == (Context *)get_expression_context(bv)) {
                bv2 = bv;
                bv_changed = false;
            } else {
                bv2 = init_var_expression_wc(get_var_name(bv), bv_type2, ctx);
                bv_changed = (bv2 != bv);
            }

            int body_min = min_depth;
            if (bv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(bv));
                dll_insert_at_tail(new_exprs, dll_new_node(bv2));
                if (bv->ctx_size < body_min) body_min = bv->ctx_size;
            }
            Expression *body2 = _td_p_subst((Context *)bv2, body, old_exprs, new_exprs, body_min);
            if (bv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            if (body2 == NULL) return NULL;

            if (!bv_changed && body2 == body) return node;
            return init_forall_expression_wc(bv2, body2);
        }

        case FIX_EXPRESSION: {
            Expression *rec_var = node->as.fix.recursive_var;
            Expression *rec_var_type = get_expression_type(rec_var);
            int arg_count = node->as.fix.arg_count;

            Expression *rec_var_type2 =
                _td_p_subst(ctx, rec_var_type, old_exprs, new_exprs, min_depth);
            if (rec_var_type2 == NULL) return NULL;
            Expression *rec_var2 =
                (rec_var_type2 == rec_var_type && ctx == (Context *)get_expression_context(rec_var))
                    ? rec_var
                    : init_var_expression_wc(get_var_name(rec_var), rec_var_type2, ctx);
            if (rec_var2 == NULL) return NULL;
            bool rv_changed = (rec_var2 != rec_var);
            int fix_min = min_depth;
            if (rv_changed) {
                dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
                dll_insert_at_tail(new_exprs, dll_new_node(rec_var2));
                if (rec_var->ctx_size < fix_min) fix_min = rec_var->ctx_size;
            }

            Expression **args2 = malloc(arg_count * sizeof(Expression *));
            bool any_arg_changed = false;
            Context *extended_ctx = (Context *)rec_var2;
            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = node->as.fix.args[i];
                Expression *old_arg_type = get_expression_type(old_arg);
                Expression *new_arg_type =
                    _td_p_subst(extended_ctx, old_arg_type, old_exprs, new_exprs, fix_min);
                if (new_arg_type == NULL) {
                    free(args2);
                    if (rv_changed) {
                        free(dll_remove_tail(old_exprs));
                        free(dll_remove_tail(new_exprs));
                    }
                    return NULL;
                }
                Expression *new_arg =
                    (new_arg_type == old_arg_type &&
                     extended_ctx == (Context *)get_expression_context(old_arg))
                        ? old_arg
                        : init_var_expression_wc(get_var_name(old_arg), new_arg_type, extended_ctx);
                if (new_arg == NULL) {
                    free(args2);
                    if (rv_changed) {
                        free(dll_remove_tail(old_exprs));
                        free(dll_remove_tail(new_exprs));
                    }
                    return NULL;
                }
                args2[i] = new_arg;
                if (new_arg != old_arg) {
                    any_arg_changed = true;
                    dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                    if (old_arg->ctx_size < fix_min) fix_min = old_arg->ctx_size;
                }
                extended_ctx = (Context *)new_arg;
            }

            Expression *body2 =
                _td_p_subst(extended_ctx, node->as.fix.body, old_exprs, new_exprs, fix_min);

            for (int i = arg_count - 1; i >= 0; i--) {
                if (args2[i] != node->as.fix.args[i]) {
                    free(dll_remove_tail(old_exprs));
                    free(dll_remove_tail(new_exprs));
                }
            }
            if (rv_changed) {
                free(dll_remove_tail(old_exprs));
                free(dll_remove_tail(new_exprs));
            }

            if (body2 == NULL) {
                free(args2);
                return NULL;
            }

            if (!rv_changed && !any_arg_changed && body2 == node->as.fix.body) {
                free(args2);
                return node;
            }
            return init_fix_expression_wc(rec_var2, args2, arg_count,
                                          node->as.fix.decreasing_arg_index, body2);
        }

        case MATCH_EXPRESSION: {
            Expression *scrutinee = node->as.match.scrutinee;
            int branch_cnt = node->as.match.branch_count;

            Expression *scrutinee2 = _td_p_subst(ctx, scrutinee, old_exprs, new_exprs, min_depth);
            if (scrutinee2 == NULL) return NULL;

            MatchBranch **branches2 = malloc(branch_cnt * sizeof(MatchBranch *));
            bool any_branch_changed = (scrutinee2 != scrutinee);

            for (int i = 0; i < branch_cnt; i++) {
                MatchBranch *br = node->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));

                br2->constructor =
                    _td_p_subst(ctx, br->constructor, old_exprs, new_exprs, min_depth);
                if (br2->constructor == NULL) {
                    free(br2);
                    for (int k = 0; k < i; k++) {
                        free(branches2[k]->pattern_variables);
                        free(branches2[k]);
                    }
                    free(branches2);
                    return NULL;
                }
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));
                Context *branch_ctx = ctx;
                int branch_min = min_depth;

                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_pv = br->pattern_variables[j];
                    Expression *old_pv_type = get_expression_type(old_pv);
                    Expression *new_pv_type =
                        _td_p_subst(branch_ctx, old_pv_type, old_exprs, new_exprs, branch_min);
                    if (new_pv_type == NULL) {
                        free(br2->pattern_variables);
                        free(br2);
                        for (int k = 0; k < i; k++) {
                            free(branches2[k]->pattern_variables);
                            free(branches2[k]);
                        }
                        free(branches2);
                        return NULL;
                    }
                    Expression *new_pv =
                        (new_pv_type == old_pv_type &&
                         branch_ctx == (Context *)get_expression_context(old_pv))
                            ? old_pv
                            : init_var_expression_wc(get_var_name(old_pv), new_pv_type, branch_ctx);
                    if (new_pv == NULL) {
                        free(br2->pattern_variables);
                        free(br2);
                        for (int k = 0; k < i; k++) {
                            free(branches2[k]->pattern_variables);
                            free(branches2[k]);
                        }
                        free(branches2);
                        return NULL;
                    }
                    br2->pattern_variables[j] = new_pv;
                    if (new_pv != old_pv) {
                        dll_insert_at_tail(old_exprs, dll_new_node(old_pv));
                        dll_insert_at_tail(new_exprs, dll_new_node(new_pv));
                        if (old_pv->ctx_size < branch_min) branch_min = old_pv->ctx_size;
                    }
                    branch_ctx = (Context *)new_pv;
                }

                br2->body = _td_p_subst(branch_ctx, br->body, old_exprs, new_exprs, branch_min);
                if (br2->body == NULL) {
                    for (int j = br->pattern_var_count - 1; j >= 0; j--) {
                        if (br2->pattern_variables[j] != br->pattern_variables[j]) {
                            free(dll_remove_tail(old_exprs));
                            free(dll_remove_tail(new_exprs));
                        }
                    }
                    free(br2->pattern_variables);
                    free(br2);
                    for (int k = 0; k < i; k++) {
                        free(branches2[k]->pattern_variables);
                        free(branches2[k]);
                    }
                    free(branches2);
                    return NULL;
                }

                for (int j = br->pattern_var_count - 1; j >= 0; j--) {
                    if (br2->pattern_variables[j] != br->pattern_variables[j]) {
                        free(dll_remove_tail(old_exprs));
                        free(dll_remove_tail(new_exprs));
                    }
                }

                if (br2->constructor != br->constructor || br2->body != br->body) {
                    any_branch_changed = true;
                }
                for (int j = 0; j < br->pattern_var_count; j++) {
                    if (br2->pattern_variables[j] != br->pattern_variables[j]) {
                        any_branch_changed = true;
                        break;
                    }
                }
                branches2[i] = br2;
            }

            if (!any_branch_changed) {
                for (int i = 0; i < branch_cnt; i++) {
                    free(branches2[i]->pattern_variables);
                    free(branches2[i]);
                }
                free(branches2);
                return node;
            }
            return init_match_expression_wc(scrutinee2, branches2, branch_cnt, ctx);
        }

        default:
            /* VAR, TYPE, PROP, HOLE: leaves - not the target, return unchanged. */
            return node;
    }
}

/* =========================================================================
 * Public API (same signatures as the uplink version)
 * ========================================================================= */

Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    int min_depth = subst_map_min_depth(old_exprs);
    return _td_p_subst(context, t, old_exprs, new_exprs, min_depth);
}

Expression *new_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                        DoublyLinkedList *new_exprs) {
    int n = dll_len(old_exprs);
    if (n != dll_len(new_exprs)) return NULL;
    if (n == 0) return t;
    int min_depth = subst_map_min_depth(old_exprs);
    return _td_p_subst(context, t, old_exprs, new_exprs, min_depth);
}

Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) return a;
    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();
    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));
    Expression *result = _td_p_subst(context, t, old_exprs, new_exprs, x->ctx_size);
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

    Context *orig = context;
    Context *fin = final_context;
    while (orig != fin && orig != x) {
        dll_insert_at_head(old_exprs, dll_new_node(orig));
        dll_insert_at_head(new_exprs, dll_new_node(fin));
        orig = get_expression_context(orig);
        fin = get_expression_context(fin);
    }

    int min_depth = subst_map_min_depth(old_exprs);
    Expression *result = _td_p_subst(final_context, t, old_exprs, new_exprs, min_depth);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    return result;
}

Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    /* For beta reduction the bound variable is always private to the lambda
       body, so new_subst and beta_subst are equivalent here. */
    return new_subst(context, t, x, a);
}

#endif  // TUNE_SUBST_TOPDOWN
