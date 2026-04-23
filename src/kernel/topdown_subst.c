#include <stdlib.h>

#include "src/common/doubly_linked_list.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/subst.h"

// #ifdef TUNE_SUBST_TOPDOWN

/*
 * _p_subst - substitute old_exprs[i] -> new_exprs[i] in t.
 *
 * context  : the result context (already cut / transformed by caller)
 * t        : expression to substitute into
 * old_exprs, new_exprs : parallel lists of equal length
 *
 * Early-exit optimisation: before descending, check whether any substitution
 * target appears in t's context chain. If none do, no target is in scope
 * anywhere in the subtree and we return t unchanged.
 */
static Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                            DoublyLinkedList *new_exprs);

static Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                            DoublyLinkedList *new_exprs) {
    /* 1. Direct hit - t itself is a substitution target. */
    DLLNode *curr_old = old_exprs->head;
    DLLNode *curr_new = new_exprs->head;
    bool any_in_scope = false;
    Context *t_ctx = get_expression_context(t);

    while (curr_old != NULL) {
        if (t == (Expression *)curr_old->data) {
            return (Expression *)curr_new->data;
        }
        if (!any_in_scope && context_find(t_ctx, (Expression *)curr_old->data) != NULL) {
            any_in_scope = true;
        }
        curr_old = curr_old->next;
        curr_new = curr_new->next;
    }

    /* 2. Early exit - no target is in scope in this subtree. */
    if (!any_in_scope) {
        return t;
    }

    switch (t->tag) {
        case VAR_EXPRESSION:
        case HOLE_EXPRESSION:
            return t;

        case LAMBDA_EXPRESSION: {
            Expression *x_bv = get_lambda_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(t);

            Expression *x_bv_type2 = _p_subst(context, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, context);

            /* Extend substitution map with binder renaming. */
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));

            Expression *body2 = _p_subst((Context *)x_bv2, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            if (x_bv2 == x_bv && body2 == body) {
                return t;
            }
            return init_lambda_expression_wc(x_bv2, body2);
        }

        case FORALL_EXPRESSION: {
            Expression *x_bv = get_forall_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(t);

            Expression *x_bv_type2 = _p_subst(context, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv2 = init_var_expression_wc(get_var_name(x_bv), x_bv_type2, context);

            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv2));

            Expression *body2 = _p_subst((Context *)x_bv2, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            if (x_bv2 == x_bv && body2 == body) {
                return t;
            }
            return init_forall_expression_wc(x_bv2, body2);
        }

        case APP_EXPRESSION: {
            Expression *func = get_app_func(t);
            Expression *arg = get_app_arg(t);
            Expression *func2 = _p_subst(context, func, old_exprs, new_exprs);
            Expression *arg2 = _p_subst(context, arg, old_exprs, new_exprs);

            if (func2 == func && arg2 == arg && valid_in_context(t, context)) {
                return t;
            }
            if (forms_beta_redex(func2, arg2)) {
                return beta_reduce(context, func2, arg2);
            }
            return init_app_expression_wc(func2, arg2, context);
        }

        case MATCH_EXPRESSION: {
            Expression *scrutinee2 = _p_subst(context, t->as.match.scrutinee, old_exprs, new_exprs);
            int branch_count = t->as.match.branch_count;
            MatchBranch **branches2 = malloc(branch_count * sizeof(MatchBranch *));
            bool any_changed = (scrutinee2 != t->as.match.scrutinee);

            for (int i = 0; i < branch_count; i++) {
                MatchBranch *br = t->as.match.branches[i];
                MatchBranch *br2 = malloc(sizeof(MatchBranch));

                br2->constructor = _p_subst(context, br->constructor, old_exprs, new_exprs);
                br2->pattern_var_count = br->pattern_var_count;
                br2->pattern_variables = malloc(br->pattern_var_count * sizeof(Expression *));

                Context *ext_ctx = context;
                for (int j = 0; j < br->pattern_var_count; j++) {
                    Expression *old_pv = br->pattern_variables[j];
                    Expression *old_pv_type = get_expression_type(old_pv);
                    Expression *new_pv_type = _p_subst(ext_ctx, old_pv_type, old_exprs, new_exprs);
                    Expression *new_pv =
                        init_var_expression_wc(get_var_name(old_pv), new_pv_type, ext_ctx);
                    br2->pattern_variables[j] = new_pv;

                    dll_insert_at_tail(old_exprs, dll_new_node(old_pv));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_pv));
                    ext_ctx = (Context *)new_pv;
                }

                br2->body = _p_subst(ext_ctx, br->body, old_exprs, new_exprs);

                /* Pop the pattern variable entries we pushed. */
                for (int j = 0; j < br->pattern_var_count; j++) {
                    dll_remove_tail(old_exprs);
                    dll_remove_tail(new_exprs);
                }

                if (br2->constructor != br->constructor || br2->body != br->body) {
                    any_changed = true;
                }
                for (int j = 0; j < br->pattern_var_count; j++) {
                    if (br2->pattern_variables[j] != br->pattern_variables[j]) {
                        any_changed = true;
                        break;
                    }
                }
                branches2[i] = br2;
            }

            if (!any_changed) {
                for (int i = 0; i < branch_count; i++) {
                    free(branches2[i]->pattern_variables);
                    free(branches2[i]);
                }
                free(branches2);
                return t;
            }
            return init_match_expression_wc(scrutinee2, branches2, branch_count, context);
        }

        case FIX_EXPRESSION: {
            Expression *rec_var = t->as.fix.recursive_var;
            Expression *rec_var_type2 =
                _p_subst(context, get_expression_type(rec_var), old_exprs, new_exprs);
            Expression *rec_var2 =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type2, context);

            dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
            dll_insert_at_tail(new_exprs, dll_new_node(rec_var2));

            int arg_count = t->as.fix.arg_count;
            Expression **args2 = malloc(arg_count * sizeof(Expression *));
            Context *ext_ctx = (Context *)rec_var2;

            for (int i = 0; i < arg_count; i++) {
                Expression *old_arg = t->as.fix.args[i];
                Expression *new_arg_type =
                    _p_subst(ext_ctx, get_expression_type(old_arg), old_exprs, new_exprs);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, ext_ctx);
                args2[i] = new_arg;
                dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                ext_ctx = (Context *)new_arg;
            }

            Expression *body2 = _p_subst(ext_ctx, t->as.fix.body, old_exprs, new_exprs);

            /* Pop args + rec_var entries. */
            for (int i = 0; i < arg_count; i++) {
                dll_remove_tail(old_exprs);
                dll_remove_tail(new_exprs);
            }
            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            if (rec_var2 == rec_var && body2 == t->as.fix.body) {
                bool args_same = true;
                for (int i = 0; i < arg_count; i++) {
                    if (args2[i] != t->as.fix.args[i]) {
                        args_same = false;
                        break;
                    }
                }
                if (args_same) {
                    free(args2);
                    return t;
                }
            }
            return init_fix_expression_wc(rec_var2, args2, arg_count,
                                          t->as.fix.decreasing_arg_index, body2);
        }

        default:
            return t;
    }
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
    return _p_subst(context, t, old_exprs, new_exprs);
}

/*
 * _subst - single substitution t[x->a].
 * Caller is responsible for context cutting; context here is the result context.
 */
Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) {
        return a;
    }

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();
    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Expression *result = _p_subst(context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    return result;
}

/*
 * new_subst - single substitution with automatic context cutting.
 *
 * Decomposes context into gamma, x:A, delta, computes delta[x->a],
 * then performs the full parallel substitution t[x->a, d_i->d_i'].
 */
Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    Context *final_context = context_cut(context, x, a);

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    /* Walk both context spines in parallel to collect delta -> delta' pairs. */
    Context *orig = context;
    Context *fin = final_context;
    while (orig != fin && orig != x) {
        dll_insert_at_head(old_exprs, dll_new_node(orig));
        dll_insert_at_head(new_exprs, dll_new_node(fin));
        orig = get_expression_context(orig);
        fin = get_expression_context(fin);
    }

    Expression *result = _p_subst(final_context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    return result;
}

/*
 * beta_subst - like new_subst but for beta reduction.
 *
 * The bound variable being substituted away is private to the lambda body
 * (no external uplinks), so context_cut is equivalent to new_subst here.
 * Kept as a separate entry point to make call sites self-documenting.
 */
Expression *beta_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    return new_subst(context, t, x, a);
}

// #endif  // TUNE_SUBST_TOPDOWN