#include "src/engine/rewrite_proof.h"
#include "src/kernel/expression.h"
#include "src/kernel/new_subst.h"
#include "src/kernel/beta_reduction.h"

Expression *new_subst(Expression *expression, Expression *old_e,
                  Expression *new_e) {
    // Optimization: we only need to perform the substitution if the expression's context contains
    // a reference to the old variable
    Context *e_ctx = get_expression_context(expression);
    if (context_find(e_ctx, old_e) == NULL) {
        return expression;
    }

    switch (expression->type) {
        case (VAR_EXPRESSION):
        case (HOLE_EXPRESSION):
            return (expression == old_e) ? new_e : expression;
        case (LAMBDA_EXPRESSION): {
            // Assume the expression has form fun (x: A) => B
            Expression *x = expression->value.lambda.bound_variable;
            Expression *A = get_expression_type(x);
            Expression *B = expression->value.lambda.body;

            // We need to first create a new binding variable for the lambda.
            // If we had (x: A), we create (x': A') where A' := A[old_e -> new_e]
            Expression *A_prime = new_subst(A, old_e, new_e);

            // Compute the minimal context for x_prime: context(A_prime) minus old_e
            DoublyLinkedList *old_list = dll_create();
            dll_insert_at_tail(old_list, dll_new_node(old_e));
            Context *x_prime_ctx = context_for_binding(A_prime, old_list);
            dll_remove_tail(old_list);
            dll_destroy(old_list);

            Expression *x_prime = init_var_expression_wc(x->value.var.name, A_prime, x_prime_ctx);

            // Next, we need to create the new body B', where B' := B[x -> x', old_e -> new_e]
            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(old_e));
            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(new_exprs, dll_new_node(new_e));
            dll_insert_at_tail(new_exprs, dll_new_node(x_prime));

            Expression *B_prime = new_p_subst(B, old_exprs, new_exprs);
            Context *B_prime_ctx = get_expression_context(B_prime);

            dll_remove_tail(old_exprs);
            dll_remove_tail(old_exprs);
            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);
            dll_remove_tail(new_exprs);
            dll_destroy(new_exprs);

            return init_lambda_expression_wc(x_prime, B_prime, B_prime_ctx);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = expression->value.app.func;
            Expression *app_arg = expression->value.app.arg;
            Expression *new_app_func = new_subst(app_func, old_e, new_e);
            Expression *new_app_arg = new_subst(app_arg, old_e, new_e);

            if (forms_redex(new_app_func, new_app_arg)) {
                return reduce(new_app_func, new_app_arg);
            } else {
                Context *app_ctx = context_add(get_expression_context(new_app_func),
                                               get_expression_context(new_app_arg));
                return init_app_expression_wc(new_app_func, new_app_arg, app_ctx);
            }
        }
        case (FORALL_EXPRESSION): {
            // Assume the expression has form forall (x: A), B
            Expression *x = expression->value.forall.bound_variable;
            Expression *A = get_expression_type(x);
            Expression *B = expression->value.forall.body;

            // We need to first create a new binding variable for the forall.
            // If we had (x: A), we create (x': A') where A' := A[old_e -> new_e]
            Expression *A_prime = new_subst(A, old_e, new_e);

            // Compute the minimal context for x_prime: context(A_prime) minus old_e
            DoublyLinkedList *old_list = dll_create();
            dll_insert_at_tail(old_list, dll_new_node(old_e));
            Context *x_prime_ctx = context_for_binding(A_prime, old_list);
            dll_remove_tail(old_list);
            dll_destroy(old_list);

            Expression *x_prime = init_var_expression_wc(x->value.var.name, A_prime, x_prime_ctx);

            // Next, we need to create the new body B', where B' := B[x -> x', old_e -> new_e]
            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(old_e));
            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(new_exprs, dll_new_node(new_e));
            dll_insert_at_tail(new_exprs, dll_new_node(x_prime));

            Expression *B_prime = new_p_subst(B, old_exprs, new_exprs);
            Context *B_prime_ctx = get_expression_context(B_prime);

            dll_remove_tail(old_exprs);
            dll_remove_tail(old_exprs);
            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);
            dll_remove_tail(new_exprs);
            dll_destroy(new_exprs);

            return init_forall_expression_wc(x_prime, B_prime, B_prime_ctx);
        }
        default:
            return expression;
    }
}

Expression *new_p_subst(Expression *expression, DoublyLinkedList *old_exprs, DoublyLinkedList *new_exprs) {
    int n = dll_len(old_exprs);
    if (n != dll_len(new_exprs)) {
        return NULL;
    }

    if (n == 0) {
        return expression;
    }

    Context *e_ctx = get_expression_context(expression);
    bool needs_substitution = false;

    DLLNode *curr_old_expr_n = old_exprs->head;
    while (curr_old_expr_n != NULL) {
        Expression *old_e = curr_old_expr_n->data;
        if (context_find(e_ctx, old_e)) {
            needs_substitution = true;
            break;
        }
        curr_old_expr_n = curr_old_expr_n->next;
    }

    if (!needs_substitution) {
        return expression;
    }

    switch (expression->type) {
        case (VAR_EXPRESSION):
        case (HOLE_EXPRESSION): {
            for (int i = 0; i < n; i++) {
                Expression *old_e = dll_at(old_exprs, i)->data;
                Expression *new_e = dll_at(new_exprs, i)->data;
                if (expression == old_e) {
                    return new_e;
                }
            }
            return expression;
        }
        case (LAMBDA_EXPRESSION): {
            // Assume expression has form fun (x: A) => B
            Expression *x = expression->value.lambda.bound_variable;
            Expression *B = expression->value.lambda.body;

            Expression *A_prime =
                new_p_subst(get_expression_type(x), old_exprs, new_exprs);

            // Compute the minimal context for x_prime: context(A_prime) minus all old variables
            Context *x_prime_ctx = context_for_binding(A_prime, old_exprs);

            Expression *x_prime =
                init_var_expression_wc(x->value.var.name, A_prime, x_prime_ctx);

            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(new_exprs, dll_new_node(x_prime));

            Expression *B_prime = new_p_subst(B, old_exprs, new_exprs);
            Context *B_prime_ctx = get_expression_context(B_prime);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_lambda_expression_wc(x_prime, B_prime, B_prime_ctx);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = expression->value.app.func;
            Expression *app_arg = expression->value.app.arg;
            Expression *new_app_func = new_p_subst(app_func, old_exprs, new_exprs);
            Expression *new_app_arg = new_p_subst(app_arg, old_exprs, new_exprs);

            if (forms_redex(new_app_func, new_app_arg)) {
                Expression *reduced = reduce(new_app_func, new_app_arg);
                return reduced;
            } else {
                Context *app_ctx = context_add(get_expression_context(new_app_func),
                                               get_expression_context(new_app_arg));
                return init_app_expression_wc(new_app_func, new_app_arg, app_ctx);
            }
        }
        case (FORALL_EXPRESSION): {
            // Assume expression has form forall (x: A), B
            Expression *x = expression->value.forall.bound_variable;
            Expression *B = expression->value.forall.body;

            Expression *A_prime =
                new_p_subst(get_expression_type(x), old_exprs, new_exprs);

            // Compute the minimal context for x_prime: context(A_prime) minus all old variables
            Context *x_prime_ctx = context_for_binding(A_prime, old_exprs);

            Expression *x_prime =
                init_var_expression_wc(x->value.var.name, A_prime, x_prime_ctx);

            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(new_exprs, dll_new_node(x_prime));

            Expression *B_prime = new_p_subst(B, old_exprs, new_exprs);
            Context *B_prime_ctx = get_expression_context(B_prime);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_forall_expression_wc(x_prime, B_prime, B_prime_ctx);
        }
        default:
            return expression;
    }
}
