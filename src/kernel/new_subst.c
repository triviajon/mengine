#include "src/kernel/new_subst.h"

#include "src/engine/rewrite_proof.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"

Expression *_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    if (t == x) {
        return a;
    }

    switch (t->tag) {
        case (VAR_EXPRESSION):
        case (HOLE_EXPRESSION):
            return t;
        case (LAMBDA_EXPRESSION): {
            // Assume the expression has form fun (x_bv: x_bv_type) => body
            Expression *x_bv = get_lambda_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(t);

            // We need to first create a new binding variable for the lambda.
            // If we had (x_bv: x_bv_type), we create (x_bv': x_bv_type') where x_bv_type' :=
            // x_bv_type[x -> a]
            Expression *x_bv_type_prime = _subst(context, x_bv_type, x, a);
            Expression *x_bv_prime =
                init_var_expression_wc(get_var_name(x_bv), x_bv_type_prime, context);

            // We now perform parallel substitution on the body.
            // The body is closed under the new_ctx := context + [x_bv': x_bv_type']
            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(a));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv_prime));

            Context *new_ctx = x_bv_prime;
            Expression *body_prime = new_p_subst(new_ctx, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(old_exprs);
            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);
            dll_remove_tail(new_exprs);
            dll_destroy(new_exprs);

            return init_lambda_expression_wc(x_bv_prime, body_prime);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = get_app_func(t);
            Expression *app_arg = get_app_arg(t);
            Expression *new_app_func = _subst(context, app_func, x, a);
            Expression *new_app_arg = _subst(context, app_arg, x, a);

            if (forms_redex(new_app_func, new_app_arg)) {
                return reduce(new_app_func, new_app_arg);
            }

            return init_app_expression_wc(new_app_func, new_app_arg, context);
        }
        case (FORALL_EXPRESSION): {
            // Assume the expression has form forall (x_bv: x_bv_type), body
            Expression *x_bv = get_forall_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(t);

            // We need to first create a new binding variable for the lambda.
            // If we had (x_bv: x_bv_type), we create (x_bv': x_bv_type') where x_bv_type' :=
            // x_bv_type[x -> a]
            Expression *x_bv_type_prime = _subst(context, x_bv_type, x, a);
            Expression *x_bv_prime =
                init_var_expression_wc(get_var_name(x_bv), x_bv_type_prime, context);

            // We now perform parallel substitution on the body.
            // The body is closed under the new_ctx := context + [x_bv': x_bv_type']
            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(a));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv_prime));

            Context *new_ctx = x_bv_prime;
            Expression *body_prime = new_p_subst(new_ctx, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(old_exprs);
            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);
            dll_remove_tail(new_exprs);
            dll_destroy(new_exprs);

            return init_forall_expression_wc(x_bv_prime, body_prime);
        }
        default:
            return t;
    }
}

Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    // First, we need to decompose context into gamma, a : A, delta. We final expression will be
    // closed under gamma, delta[x -> a]:
    Context *final_context = context_cut(context, x, a);
    Expression *final_expression = _subst(final_context, t, x, a);
    return final_expression;
}

Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    // Check if t is one of the expressions to be replaced
    DLLNode *curr_old = old_exprs->head;
    DLLNode *curr_new = new_exprs->head;
    while (curr_old != NULL) {
        if (t == curr_old->data) {
            return curr_new->data;
        }
        curr_old = curr_old->next;
        curr_new = curr_new->next;
    }

    switch (t->tag) {
        case (VAR_EXPRESSION):
        case (HOLE_EXPRESSION):
            return t;
        case (LAMBDA_EXPRESSION): {
            // Assume the expression has form fun (x_bv: x_bv_type) => body
            Expression *x_bv = get_lambda_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_lambda_body(t);

            // We need to first create a new binding variable for the lambda.
            // If we had (x_bv: x_bv_type), we create (x_bv': x_bv_type') where x_bv_type' :=
            // x_bv_type[old_exprs -> new_exprs]
            Expression *x_bv_type_prime = _p_subst(context, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv_prime =
                init_var_expression_wc(get_var_name(x_bv), x_bv_type_prime, context);

            // We now perform parallel substitution on the body.
            // The body is closed under the new_ctx := context + [x_bv': x_bv_type']
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv_prime));

            Context *new_ctx = x_bv_prime;
            Expression *body_prime = _p_subst(new_ctx, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_lambda_expression_wc(x_bv_prime, body_prime);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = get_app_func(t);
            Expression *app_arg = get_app_arg(t);
            Expression *new_app_func = _p_subst(context, app_func, old_exprs, new_exprs);
            Expression *new_app_arg = _p_subst(context, app_arg, old_exprs, new_exprs);

            if (forms_redex(new_app_func, new_app_arg)) {
                return reduce(new_app_func, new_app_arg);
            }

            return init_app_expression_wc(new_app_func, new_app_arg, context);
        }
        case (FORALL_EXPRESSION): {
            // Assume the expression has form forall (x_bv: x_bv_type), body
            Expression *x_bv = get_forall_bound_variable(t);
            Expression *x_bv_type = get_expression_type(x_bv);
            Expression *body = get_forall_body(t);

            // We need to first create a new binding variable for the forall.
            // If we had (x_bv: x_bv_type), we create (x_bv': x_bv_type') where x_bv_type' :=
            // x_bv_type[old_exprs -> new_exprs]
            Expression *x_bv_type_prime = _p_subst(context, x_bv_type, old_exprs, new_exprs);
            Expression *x_bv_prime =
                init_var_expression_wc(get_var_name(x_bv), x_bv_type_prime, context);

            // We now perform parallel substitution on the body.
            // The body is closed under the new_ctx := context + [x_bv': x_bv_type']
            dll_insert_at_tail(old_exprs, dll_new_node(x_bv));
            dll_insert_at_tail(new_exprs, dll_new_node(x_bv_prime));

            Context *new_ctx = x_bv_prime;
            Expression *body_prime = _p_subst(new_ctx, body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_forall_expression_wc(x_bv_prime, body_prime);
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
