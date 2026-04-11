#include "src/kernel/subst.h"

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

            dll_destroy(old_exprs);
            dll_destroy(new_exprs);

            return init_lambda_expression_wc(x_bv_prime, body_prime);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = get_app_func(t);
            Expression *app_arg = get_app_arg(t);
            Expression *new_app_func = _subst(context, app_func, x, a);
            Expression *new_app_arg = _subst(context, app_arg, x, a);

            if (forms_beta_redex(new_app_func, new_app_arg)) {
                return beta_reduce(context, new_app_func, new_app_arg);
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

            dll_destroy(old_exprs);
            dll_destroy(new_exprs);

            return init_forall_expression_wc(x_bv_prime, body_prime);
        }
        case (MATCH_EXPRESSION): {
            // Substitute in scrutinee
            Expression *scrutinee_prime = _subst(context, t->as.match.scrutinee, x, a);

            // Create new branches with substitution
            int branch_count = t->as.match.branch_count;
            MatchBranch **branches_prime = malloc(branch_count * sizeof(MatchBranch *));

            for (int i = 0; i < branch_count; i++) {
                MatchBranch *branch = t->as.match.branches[i];
                MatchBranch *branch_prime = malloc(sizeof(MatchBranch));

                // Substitute constructor
                branch_prime->constructor = _subst(context, branch->constructor, x, a);
                branch_prime->pattern_var_count = branch->pattern_var_count;

                // Create new pattern variables with substituted types
                branch_prime->pattern_variables =
                    malloc(branch->pattern_var_count * sizeof(Expression *));

                DoublyLinkedList *old_exprs = dll_create();
                DoublyLinkedList *new_exprs = dll_create();
                dll_insert_at_tail(old_exprs, dll_new_node(x));
                dll_insert_at_tail(new_exprs, dll_new_node(a));

                Context *extended_context = context;
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *old_var = branch->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);
                    Expression *new_var_type =
                        new_p_subst(extended_context, old_var_type, old_exprs, new_exprs);
                    Expression *new_var = init_var_expression_wc(get_var_name(old_var),
                                                                 new_var_type, extended_context);

                    branch_prime->pattern_variables[j] = new_var;

                    dll_insert_at_tail(old_exprs, dll_new_node(old_var));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_var));
                    extended_context = new_var;
                }

                // Substitute in branch body
                branch_prime->body =
                    new_p_subst(extended_context, branch->body, old_exprs, new_exprs);

                dll_destroy(old_exprs);
                dll_destroy(new_exprs);
                branches_prime[i] = branch_prime;
            }

            return init_match_expression_wc(scrutinee_prime, branches_prime, branch_count, context);
        }
        case (FIX_EXPRESSION): {
            // Substitute in recursive var type
            Expression *rec_var = t->as.fix.recursive_var;
            Expression *rec_var_type = get_expression_type(rec_var);
            Expression *rec_var_type_prime = _subst(context, rec_var_type, x, a);
            Expression *rec_var_prime =
                init_var_expression_wc(get_var_name(rec_var), rec_var_type_prime, context);

            // Create new args with substituted types
            Expression **args_prime = malloc(t->as.fix.arg_count * sizeof(Expression *));

            DoublyLinkedList *old_exprs = dll_create();
            DoublyLinkedList *new_exprs = dll_create();
            dll_insert_at_tail(old_exprs, dll_new_node(x));
            dll_insert_at_tail(old_exprs, dll_new_node(rec_var));
            dll_insert_at_tail(new_exprs, dll_new_node(a));
            dll_insert_at_tail(new_exprs, dll_new_node(rec_var_prime));

            Context *extended_context = context;
            for (int i = 0; i < t->as.fix.arg_count; i++) {
                Expression *old_arg = t->as.fix.args[i];
                Expression *old_arg_type = get_expression_type(old_arg);
                Expression *new_arg_type =
                    new_p_subst(extended_context, old_arg_type, old_exprs, new_exprs);
                Expression *new_arg =
                    init_var_expression_wc(get_var_name(old_arg), new_arg_type, extended_context);

                args_prime[i] = new_arg;

                dll_insert_at_tail(old_exprs, dll_new_node(old_arg));
                dll_insert_at_tail(new_exprs, dll_new_node(new_arg));
                extended_context = new_arg;
            }

            // Substitute in body
            Expression *body_prime =
                new_p_subst(extended_context, t->as.fix.body, old_exprs, new_exprs);

            dll_destroy(old_exprs);
            dll_destroy(new_exprs);

            return init_fix_expression_wc(rec_var_prime, args_prime, t->as.fix.arg_count,
                                          t->as.fix.decreasing_arg_index, body_prime);
        }
        default:
            return t;
    }
}

Expression *new_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    // First, we need to decompose context into gamma, a : A, delta. We final expression will be
    // closed under gamma, delta[x -> a]:
    Context *final_context = context_cut(context, x, a);

    // Now we have two contexts:
    // original := *[x0: A0] ... [xn: An] [x: A] [d0 : D0 ] ... [dm : Dm ]
    // final    := *[x0: A0] ... [xn: An]        [d0': D0'] ... [dm': Dm']
    // in addition to kicking off the x -> a substitution, we need to start a parallel substitution
    // with [d0 -> d0'] ... [dm -> dm']

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    dll_insert_at_tail(old_exprs, dll_new_node(x));
    dll_insert_at_tail(new_exprs, dll_new_node(a));

    Context *original_context_c = context;
    Context *final_context_c = final_context;
    // The two contexts are the same again at [xn: An], so that's our stopping condition
    while (original_context_c != final_context_c && original_context_c != x) {
        dll_insert_at_head(old_exprs, dll_new_node(original_context_c));
        dll_insert_at_head(new_exprs, dll_new_node(final_context_c));

        original_context_c = get_expression_context(original_context_c);
        final_context_c = get_expression_context(final_context_c);
    }

    Expression *result = new_p_subst(final_context, t, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);

    return result;
}

Expression *_p_subst(Context *context, Expression *t, DoublyLinkedList *old_exprs,
                     DoublyLinkedList *new_exprs) {
    // Check if t is one of the expressions to be replaced
    // We also check that at least one of the old_exprs occurs in the context.
    // This is a very important optimization!
    DLLNode *curr_old = old_exprs->head;
    DLLNode *curr_new = new_exprs->head;
    bool gt_0_occured = false;
    Context *t_ctx = get_expression_context(t);
    while (curr_old != NULL) {
        if (t == curr_old->data) {
            return curr_new->data;
        }

        if (!gt_0_occured && context_find(t_ctx, curr_old->data) != NULL) {
            gt_0_occured = true;
        }

        curr_old = curr_old->next;
        curr_new = curr_new->next;
    }

    if (!gt_0_occured) {
        return t;
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

            free(dll_remove_tail(old_exprs));
            free(dll_remove_tail(new_exprs));

            return init_lambda_expression_wc(x_bv_prime, body_prime);
        }
        case (APP_EXPRESSION): {
            Expression *app_func = get_app_func(t);
            Expression *app_arg = get_app_arg(t);
            Expression *new_app_func = _p_subst(context, app_func, old_exprs, new_exprs);
            Expression *new_app_arg = _p_subst(context, app_arg, old_exprs, new_exprs);

            if (forms_beta_redex(new_app_func, new_app_arg)) {
                return beta_reduce(context, new_app_func, new_app_arg);
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

            free(dll_remove_tail(old_exprs));
            free(dll_remove_tail(new_exprs));

            return init_forall_expression_wc(x_bv_prime, body_prime);
        }
        case (MATCH_EXPRESSION): {
            // Substitute in scrutinee
            Expression *scrutinee = t->as.match.scrutinee;
            Expression *scrutinee_prime = _p_subst(context, scrutinee, old_exprs, new_exprs);

            // Create new branches with substituted components
            int branch_count = t->as.match.branch_count;
            MatchBranch **branches_prime = malloc(branch_count * sizeof(MatchBranch *));

            for (int i = 0; i < branch_count; i++) {
                MatchBranch *branch = t->as.match.branches[i];
                MatchBranch *branch_prime = malloc(sizeof(MatchBranch));

                // Substitute constructor
                branch_prime->constructor =
                    _p_subst(context, branch->constructor, old_exprs, new_exprs);
                branch_prime->pattern_var_count = branch->pattern_var_count;

                // Create new pattern variables with substituted types
                branch_prime->pattern_variables =
                    malloc(branch->pattern_var_count * sizeof(Expression *));

                Context *extended_context = context;

                for (int j = 0; j < branch->pattern_var_count; j++) {
                    Expression *old_var = branch->pattern_variables[j];
                    Expression *old_var_type = get_expression_type(old_var);
                    Expression *new_var_type =
                        _p_subst(extended_context, old_var_type, old_exprs, new_exprs);
                    Expression *new_var = init_var_expression_wc(get_var_name(old_var),
                                                                 new_var_type, extended_context);

                    branch_prime->pattern_variables[j] = new_var;

                    // Add to substitution lists for body substitution
                    dll_insert_at_tail(old_exprs, dll_new_node(old_var));
                    dll_insert_at_tail(new_exprs, dll_new_node(new_var));

                    // Update context for next variable
                    extended_context = new_var;
                }

                // Substitute in branch body with extended context
                branch_prime->body = _p_subst(extended_context, branch->body, old_exprs, new_exprs);

                // Remove pattern variables from substitution lists
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    free(dll_remove_tail(old_exprs));
                    free(dll_remove_tail(new_exprs));
                }

                branches_prime[i] = branch_prime;
            }

            return init_match_expression_wc(scrutinee_prime, branches_prime, branch_count, context);
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
