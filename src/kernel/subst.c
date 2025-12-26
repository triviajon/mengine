#include "src/kernel/subst.h"

#include "src/kernel/beta_reduction.h"

Expression *subst(Expression *expression, Expression *old_e,
                  Expression *new_e) {
    Context *e_ctx = get_expression_context(expression);
    if (context_find(e_ctx, old_e) == NULL) {
        return expression;
    }

    switch (expression->type) {
        case (VAR_EXPRESSION):
        case (HOLE_EXPRESSION):
            return (expression == old_e) ? new_e : expression;
        case (LAMBDA_EXPRESSION): {
            Expression *lambda_var = expression->value.lambda.bound_variable;
            Expression *lambda_var_ty = get_expression_type(lambda_var);
            Expression *lambda_body = expression->value.lambda.body;
            Context *lambda_body_ctx = get_expression_context(lambda_body);

            if (context_find(get_expression_context(lambda_var_ty), old_e) ==
                NULL) {
                Expression *new_body = subst(lambda_body, old_e, new_e);
                return init_lambda_expression(lambda_var, new_body);
            }

            Expression *new_lambda_var_type =
                subst(lambda_var_ty, old_e, new_e);
            Expression *new_lambda_var = init_var_expression(
                lambda_var->value.var.name, new_lambda_var_type);

            if (context_find(lambda_body_ctx, lambda_var) == NULL) {
                Expression *new_body = subst(lambda_body, old_e, new_e);
                return init_lambda_expression(new_lambda_var, new_body);
            }
            DoublyLinkedList *old_exprs = dll_create();

            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(old_e));

            dll_insert_at_tail(old_exprs, dll_new_node(lambda_var));

            dll_insert_at_tail(new_exprs, dll_new_node(new_e));

            dll_insert_at_tail(new_exprs, dll_new_node(new_lambda_var));

            Expression *new_body =

                p_subst(lambda_body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);

            dll_remove_tail(old_exprs);

            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);

            dll_remove_tail(new_exprs);

            dll_destroy(new_exprs);

            return init_lambda_expression(new_lambda_var, new_body);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = expression->value.app.func;
            Expression *app_arg = expression->value.app.arg;
            Expression *new_app_func =
                (context_find(get_expression_context(app_func), old_e) == NULL)
                    ? app_func
                    : subst(app_func, old_e, new_e);
            Expression *new_app_arg =
                (context_find(get_expression_context(app_arg), old_e) == NULL)
                    ? app_arg
                    : subst(app_arg, old_e, new_e);

            if ((app_func == new_app_func) && (app_arg == new_app_arg)) {
                return expression;
            }

            if (forms_redex(new_app_func, new_app_arg)) {
                return reduce(new_app_func, new_app_arg);
            }
            Expression *result = init_app_expression(new_app_func, new_app_arg);
            if (!result) {
                return NULL;
            }
            return result;
        }
        case (FORALL_EXPRESSION): {
            Expression *forall_var = expression->value.forall.bound_variable;
            Expression *forall_var_ty = get_expression_type(forall_var);

            Expression *forall_body = expression->value.forall.body;
            Context *forall_body_ctx = get_expression_context(forall_body);
            if (context_find(get_expression_context(forall_var_ty), old_e) ==
                NULL) {
                Expression *new_body = subst(forall_body, old_e, new_e);
                return init_forall_expression(forall_var, new_body);
            }

            Expression *new_forall_var_type =
                subst(forall_var_ty, old_e, new_e);
            Expression *new_forall_var = init_var_expression(
                forall_var->value.var.name, new_forall_var_type);

            if (context_find(forall_body_ctx, forall_var) == NULL) {
                Expression *new_body = subst(forall_body, old_e, new_e);
                return init_forall_expression(new_forall_var, new_body);
            }
            DoublyLinkedList *old_exprs = dll_create();

            DoublyLinkedList *new_exprs = dll_create();

            dll_insert_at_tail(old_exprs, dll_new_node(old_e));

            dll_insert_at_tail(old_exprs, dll_new_node(forall_var));

            dll_insert_at_tail(new_exprs, dll_new_node(new_e));

            dll_insert_at_tail(new_exprs, dll_new_node(new_forall_var));

            Expression *new_body =

                p_subst(forall_body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);

            dll_remove_tail(old_exprs);

            dll_destroy(old_exprs);

            dll_remove_tail(new_exprs);

            dll_remove_tail(new_exprs);

            dll_destroy(new_exprs);

            return init_forall_expression(new_forall_var, new_body);
        }
        case (TYPE_EXPRESSION):
            return expression;
        case (PROP_EXPRESSION):
            return expression;
    }
}

Expression *p_subst(Expression *expression, DoublyLinkedList *old_exprs,
                    DoublyLinkedList *new_exprs) {
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
            Expression *lambda_var = expression->value.lambda.bound_variable;
            Expression *lambda_var_ty = get_expression_type(lambda_var);
            Expression *new_lambda_var_type =
                p_subst(lambda_var_ty, old_exprs, new_exprs);
            char *new_var_name = malloc(strlen(lambda_var->value.var.name) + 2);
            strcpy(new_var_name, lambda_var->value.var.name);
            strcat(new_var_name, "'");
            Expression *new_lambda_var =
                init_var_expression(new_var_name, new_lambda_var_type);

            dll_insert_at_tail(old_exprs, dll_new_node(lambda_var));
            dll_insert_at_tail(new_exprs, dll_new_node(new_lambda_var));

            Expression *lambda_body = expression->value.lambda.body;
            Expression *new_body = p_subst(lambda_body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_lambda_expression(new_lambda_var, new_body);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = expression->value.app.func;
            Expression *app_arg = expression->value.app.arg;
            Expression *new_app_func = p_subst(app_func, old_exprs, new_exprs);
            Expression *new_app_arg = p_subst(app_arg, old_exprs, new_exprs);

            if ((app_func == new_app_func) && (app_arg == new_app_arg)) {
                return expression;
            }

            if (forms_redex(new_app_func, new_app_arg)) {
                Expression *reduced = reduce(new_app_func, new_app_arg);
                return reduced;
            }
            Expression *result = init_app_expression(new_app_func, new_app_arg);
            if (!result) {
                return NULL;
            }
            return result;
        }
        case (FORALL_EXPRESSION): {
            Expression *forall_var = expression->value.forall.bound_variable;
            int search_result = dll_search_for_idx(old_exprs, forall_var);
            if (search_result != -1) {
                Expression *new_forall_var =
                    dll_at(new_exprs, search_result)->data;
                Expression *forall_body = expression->value.forall.body;

                return init_forall_expression(
                    new_forall_var, p_subst(forall_body, old_exprs, new_exprs));
            }
            Expression *forall_var_ty = get_expression_type(forall_var);
            Expression *new_forall_var_type =
                p_subst(forall_var_ty, old_exprs, new_exprs);
            Expression *new_forall_var = init_var_expression(
                forall_var->value.var.name, new_forall_var_type);

            dll_insert_at_tail(old_exprs, dll_new_node(forall_var));
            dll_insert_at_tail(new_exprs, dll_new_node(new_forall_var));

            Expression *forall_body = expression->value.forall.body;
            Expression *new_body = p_subst(forall_body, old_exprs, new_exprs);

            dll_remove_tail(old_exprs);
            dll_remove_tail(new_exprs);

            return init_forall_expression(new_forall_var, new_body);
        }
        case (TYPE_EXPRESSION):
            return expression;
        case (PROP_EXPRESSION):
            return expression;
    }
}
