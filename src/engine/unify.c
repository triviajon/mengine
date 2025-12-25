#include "unify.h"

#include "src/kernel/subst.h"
#include "src/runtime/core.h"

Expression *instantiate_lemma_type(Context *context, Expression *lemma_ty) {
    switch (lemma_ty->type) {
        case (FORALL_EXPRESSION): {
            Expression *bound_var = lemma_ty->value.forall.bound_variable;
            Expression *bound_var_ty = get_expression_type(bound_var);
            Expression *hole = init_hole_expression(bound_var->value.var.name,
                                                    bound_var_ty, context);
            Expression *lemma_ty_body = lemma_ty->value.forall.body;
            Expression *new_body = subst(lemma_ty_body, bound_var, hole);
            // The result of inner_inst should be the body of lemma with binding
            // variables substituted for holes
            Expression *inner_inst = instantiate_lemma_type(context, new_body);
            return inner_inst;
        }
        default:
            return lemma_ty;
    }
}

Expression *instantiate_lemma(Context *context, Expression *lemma) {
    Expression *lemma_ty = get_expression_type(lemma);
    return instantiate_lemma_type(context, lemma_ty);
}

DoublyLinkedList *_list_holes(Expression *expr, DoublyLinkedList *curr) {
    switch (expr->type) {
        case (LAMBDA_EXPRESSION): {
            curr = _list_holes(
                expr->value.lambda.bound_variable->value.var.type, curr);
            curr = _list_holes(expr->value.lambda.body, curr);
            return curr;
        }
        case (FORALL_EXPRESSION): {
            curr = _list_holes(
                expr->value.forall.bound_variable->value.var.type, curr);
            curr = _list_holes(expr->value.forall.body, curr);
            return curr;
        }
        case (APP_EXPRESSION): {
            curr = _list_holes(expr->value.app.func, curr);
            curr = _list_holes(expr->value.app.arg, curr);
            return curr;
        }
        case (HOLE_EXPRESSION): {
            if (dll_search(curr, expr) == NULL) {
                dll_insert_at_tail(curr, dll_new_node(expr));
                return curr;
            }
            return curr;
        }
        default:
            return curr;
    }
}

DoublyLinkedList *list_holes(Expression *expr) {
    DoublyLinkedList *lst = dll_create();
    return _list_holes(expr, lst);
}

int num_holes(Expression *expr) {
    DoublyLinkedList *lst = list_holes(expr);
    int num_holes = dll_len(lst);
    dll_destroy(lst);
    return num_holes;
}

Expression *_unify2(Expression *exprA, Expression *exprB,
                    Expression *var_to_fill) {
    if (exprA == exprB) {
        return NULL;
    }

    switch (exprA->type) {
        case VAR_EXPRESSION: {
            if (exprA != var_to_fill) {
                // Don't care...
                return NULL;
            }

            Expression *exprB_ty = get_expression_type(exprB);
            Expression *expected_ty = var_to_fill->value.var.type;
            if (congruence(exprB_ty, expected_ty)) {
                return exprB;
            }
            return NULL;
        }
        case APP_EXPRESSION: {
            if (exprB->type != APP_EXPRESSION) {
                return NULL;
            }

            Expression *new_func = _unify2(exprA->value.app.func,
                                           exprB->value.app.func, var_to_fill);
            if (new_func != NULL && new_func->type != HOLE_EXPRESSION) {
                return new_func;
            }

            Expression *new_arg = _unify2(exprA->value.app.arg,
                                          exprB->value.app.arg, var_to_fill);
            if (new_arg != NULL) {
                return new_arg;
            }

            return new_func;
        }
        default:
            return NULL;
    }
}

Expression *instantiate_lemma_with_bindings(Expression *lemma,
                                            Expression *lemma_ty,
                                            Map *binders) {
    Expression *final_expr = lemma;
    Expression *curr_forall = lemma_ty;
    while (curr_forall->type == FORALL_EXPRESSION) {
        Expression *binding_var = curr_forall->value.forall.bound_variable;
        Expression *binding_result = map_get(binders, binding_var);
        final_expr = init_app_expression(final_expr, binding_result);
        Expression *curr_forall_body = curr_forall->value.forall.body;
        curr_forall = subst(curr_forall_body, binding_var, binding_result);
    }
    return final_expr;
}

UnificationResult *init_unification_result(Expression *lemma_instantiation,
                                           DoublyLinkedList *new_goals) {
    UnificationResult *unification_result =
        (UnificationResult *)malloc(sizeof(UnificationResult));
    unification_result->lemma_instantiation = lemma_instantiation;
    unification_result->new_goals = new_goals;
    return unification_result;
}

void free_unification_result(UnificationResult *unification_result) {
    if (unification_result) {
        free(unification_result);
    }
}

/**
 * Tries to unify a given lemma with an expression by instantiating the
 * universally quantified variables and attempting to find appropriate
 * substitutions. Returns a list of any remaining unfilled holes and the lemma
 * instantiation.
 */
UnificationResult *eunify2(Expression *lemma, Expression *goal) {
    Context *goal_context = get_expression_context(goal);
    Expression *expr = get_expression_type(goal);

    Expression *current_lemma_app = lemma;
    Expression *current_lemma_app_ty = get_expression_type(current_lemma_app);
    DoublyLinkedList *remaining_open = dll_create();
    while (current_lemma_app_ty->type == FORALL_EXPRESSION) {
        Expression *bound_variable =
            current_lemma_app_ty->value.forall.bound_variable;
        Expression *hole_subst = _unify2(
            get_innermost_body(current_lemma_app_ty), expr, bound_variable);

        if (hole_subst == NULL) {
            Expression *hole_to_fill = init_hole_expression(
                bound_variable->value.var.name,
                get_expression_type(bound_variable), goal_context);
            current_lemma_app =
                init_app_expression(current_lemma_app, hole_to_fill);
            dll_insert_at_tail(remaining_open, dll_new_node(hole_to_fill));
        } else {
            current_lemma_app =
                init_app_expression(current_lemma_app, hole_subst);
        }
        current_lemma_app_ty = get_expression_type(current_lemma_app);
    }
    return init_unification_result(current_lemma_app, remaining_open);
}

UnificationResult *bad_unify_for_eq(Context *goal_context, Expression *lemma,
                                    Expression *expr) {
    Expression *current_lemma_app = lemma;
    Expression *current_lemma_app_ty = get_expression_type(current_lemma_app);
    DoublyLinkedList *remaining_open = dll_create();
    while (current_lemma_app_ty->type == FORALL_EXPRESSION) {
        Expression *current_lemma_ty_lhs =
            _get_lhs_eq(get_innermost_body(current_lemma_app_ty));
        Expression *bound_variable =
            current_lemma_app_ty->value.forall.bound_variable;
        Expression *hole_subst =
            _unify2(current_lemma_ty_lhs, expr, bound_variable);

        if (hole_subst == NULL) {
            Expression *hole_to_fill = init_hole_expression(
                bound_variable->value.var.name,
                get_expression_type(bound_variable), goal_context);
            current_lemma_app =
                init_app_expression(current_lemma_app, hole_to_fill);
            dll_insert_at_tail(remaining_open, dll_new_node(hole_to_fill));
        } else {
            current_lemma_app =
                init_app_expression(current_lemma_app, hole_subst);
        }
        current_lemma_app_ty = get_expression_type(current_lemma_app);
    }
    return init_unification_result(current_lemma_app, remaining_open);
}