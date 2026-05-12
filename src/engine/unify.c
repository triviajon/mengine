#include "unify.h"

#include <stdlib.h>

#include "src/kernel/kernel_api.h"
#include "src/runtime/core.h"

static Context *join_contexts(Context *a, Context *b) {
    if (kernel_context_is_ancestor(a, b)) {
        return b;
    }
    if (kernel_context_is_ancestor(b, a)) {
        return a;
    }
    return a;
}

Expression *instantiate_lemma_type(Context *context, Expression *lemma_ty) {
    if (kernel_expr_is_forall(lemma_ty)) {
        Expression *bound_var = kernel_forall_var(lemma_ty);
        Expression *bound_var_ty = kernel_expr_type(bound_var);
        Expression *hole = kernel_hole_create(kernel_var_name(bound_var), bound_var_ty, context);
        Expression *lemma_ty_body = kernel_forall_body(lemma_ty);
        // lemma_ty_body is closed under context(bound_var), which contains bound_var
        Expression *new_body = kernel_subst(bound_var, lemma_ty_body, bound_var, hole);
        // The result of inner_inst should be the body of lemma with binding
        // variables substituted for holes
        Expression *inner_inst = instantiate_lemma_type(context, new_body);
        return inner_inst;
    }
    return lemma_ty;
}

Expression *instantiate_lemma(Context *context, Expression *lemma) {
    Expression *lemma_ty = kernel_expr_type(lemma);
    return instantiate_lemma_type(context, lemma_ty);
}

DoublyLinkedList *_list_holes(Expression *expr, DoublyLinkedList *curr) {
    if (kernel_expr_is_lambda(expr)) {
        curr = _list_holes(kernel_expr_type(kernel_lambda_var(expr)), curr);
        curr = _list_holes(kernel_lambda_body(expr), curr);
    } else if (kernel_expr_is_forall(expr)) {
        curr = _list_holes(kernel_expr_type(kernel_forall_var(expr)), curr);
        curr = _list_holes(kernel_forall_body(expr), curr);
    } else if (kernel_expr_is_app(expr)) {
        curr = _list_holes(kernel_app_func(expr), curr);
        curr = _list_holes(kernel_app_arg(expr), curr);
    } else if (kernel_expr_is_hole(expr) && dll_search(curr, expr) == NULL) {
        dll_insert_at_tail(curr, dll_new_node(expr));
    }
    return curr;
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

Expression *_unify2(Expression *exprA, Expression *exprB, Expression *var_to_fill) {
    exprA = kernel_normalize_whnf(exprA);
    exprB = kernel_normalize_whnf(exprB);

    if (exprA == exprB) {
        return NULL;
    }

    if (kernel_expr_is_var(exprA)) {
        if (exprA != var_to_fill) {
            // Don't care...
            return NULL;
        }

        Expression *exprB_ty = kernel_expr_type(exprB);
        Expression *expected_ty = kernel_expr_type(var_to_fill);
        if (kernel_expr_congruent(exprB_ty, expected_ty)) {
            return exprB;
        }
        return NULL;
    }
    if (kernel_expr_is_app(exprA)) {
        if (!kernel_expr_is_app(exprB)) {
            return NULL;
        }

        Expression *new_func =
            _unify2(kernel_app_func(exprA), kernel_app_func(exprB), var_to_fill);
        if (new_func != NULL && !kernel_expr_is_hole(new_func)) {
            return new_func;
        }

        Expression *new_arg =
            _unify2(kernel_app_arg(exprA), kernel_app_arg(exprB), var_to_fill);
        if (new_arg != NULL) {
            return new_arg;
        }

        return new_func;
    }
    return NULL;
}

Expression *instantiate_lemma_with_bindings(Expression *lemma, Expression *lemma_ty,
                                            LinearMap *binders) {
    Expression *final_expr = lemma;
    // Walk the original forall chain structurally instead of substituting at each step.
    // init_app_expression_wc already computes the correct type via _construct_app_type;
    // the redundant explicit new_subst for curr_forall is eliminated.
    Expression *curr_forall = lemma_ty;
    while (kernel_expr_is_forall(curr_forall)) {
        Expression *binding_var = kernel_forall_var(curr_forall);
        Expression *binding_result = linear_map_get(binders, binding_var);
        Context *app_ctx = join_contexts(kernel_expr_context(final_expr),
                                         kernel_expr_context(binding_result));
        final_expr = kernel_app_create(final_expr, binding_result, app_ctx);
        curr_forall = kernel_forall_body(curr_forall);  // structural advance, no substitution
    }
    return final_expr;
}

UnificationResult *init_unification_result(Expression *lemma_instantiation,
                                           DoublyLinkedList *new_goals) {
    UnificationResult *unification_result = (UnificationResult *)malloc(sizeof(UnificationResult));
    unification_result->lemma_instantiation = lemma_instantiation;
    unification_result->new_goals = new_goals;
    return unification_result;
}

void free_unification_result(UnificationResult *unification_result) {
    if (unification_result) {
        if (unification_result->new_goals) {
            dll_destroy(unification_result->new_goals);
        }
        free(unification_result);
    }
}

UnificationResult *eunify2(Expression *lemma, Expression *goal) {
    Context *goal_context = kernel_expr_context(goal);
    Expression *expr = kernel_normalize_whnf(kernel_expr_type(goal));

    Expression *current_lemma_app = lemma;
    Expression *current_lemma_app_ty = kernel_expr_type(current_lemma_app);
    DoublyLinkedList *remaining_open = dll_create();
    while (kernel_expr_is_forall(current_lemma_app_ty)) {
        Expression *bound_variable = kernel_forall_var(current_lemma_app_ty);
        Expression *lemma_target =
            kernel_normalize_whnf(kernel_expr_innermost_body(current_lemma_app_ty));
        Expression *hole_subst = _unify2(lemma_target, expr, bound_variable);

        if (hole_subst == NULL) {
            Expression *hole_to_fill = kernel_hole_create(
                kernel_var_name(bound_variable), kernel_expr_type(bound_variable), goal_context);
            if (!hole_to_fill) {
                dll_destroy(remaining_open);
                return NULL;
            }
            current_lemma_app = kernel_app_create(current_lemma_app, hole_to_fill, goal_context);
            dll_insert_at_tail(remaining_open, dll_new_node(hole_to_fill));
        } else {
            current_lemma_app = kernel_app_create(current_lemma_app, hole_subst, goal_context);
        }
        if (!current_lemma_app) {
            dll_destroy(remaining_open);
            return NULL;
        }
        current_lemma_app_ty = kernel_expr_type(current_lemma_app);
    }

    // Shelve holes that appear in other holes' types (evar-like arguments).
    // These will be resolved via cascade fill when a dependent goal is solved.
    DLLNode *node = remaining_open->head;
    while (node != NULL) {
        Expression *hole = (Expression *)node->data;
        bool appears_in_other = false;
        DLLNode *other = remaining_open->head;
        while (other != NULL) {
            if (other != node) {
                Expression *other_hole = (Expression *)other->data;
                Expression *other_type = kernel_expr_type(other_hole);
                DoublyLinkedList *other_type_holes = list_holes(other_type);
                if (dll_search(other_type_holes, hole) != NULL) {
                    appears_in_other = true;
                }
                dll_destroy(other_type_holes);
                if (appears_in_other) {
                    break;
                }
            }
            other = other->next;
        }
        if (appears_in_other) {
            kernel_hole_mark_satisfied(hole);
        }
        node = node->next;
    }

    return init_unification_result(current_lemma_app, remaining_open);
}

UnificationResult *bad_unify_for_eq(Context *goal_context, Expression *lemma, Expression *expr) {
    Expression *current_lemma_app = lemma;
    Expression *current_lemma_app_ty = kernel_expr_type(current_lemma_app);
    DoublyLinkedList *remaining_open = dll_create();
    while (kernel_expr_is_forall(current_lemma_app_ty)) {
        Expression *current_lemma_ty_lhs =
            _get_lhs_eq(kernel_expr_innermost_body(current_lemma_app_ty));
        Expression *bound_variable = kernel_forall_var(current_lemma_app_ty);
        Expression *hole_subst = _unify2(current_lemma_ty_lhs, expr, bound_variable);

        if (hole_subst == NULL) {
            Expression *hole_to_fill = kernel_hole_create(
                kernel_var_name(bound_variable), kernel_expr_type(bound_variable), goal_context);
            if (!hole_to_fill) {
                dll_destroy(remaining_open);
                return NULL;
            }
            current_lemma_app = kernel_app_create(current_lemma_app, hole_to_fill, goal_context);
            dll_insert_at_tail(remaining_open, dll_new_node(hole_to_fill));
        } else {
            current_lemma_app = kernel_app_create(current_lemma_app, hole_subst, goal_context);
        }
        if (!current_lemma_app) {
            dll_destroy(remaining_open);
            return NULL;
        }
        current_lemma_app_ty = kernel_expr_type(current_lemma_app);
    }
    return init_unification_result(current_lemma_app, remaining_open);
}
UnificationResult *unify_and_instantiate(Context *goal_context, Expression *lemma,
                                         Expression *lemma_ty, Expression *expr) {
    (void)lemma_ty;  // Not used in current implementation
    (void)expr;      // Not used in current implementation

    // Instantiate the lemma with holes in the goal context
    Expression *instantiated_lemma = instantiate_lemma(goal_context, lemma);

    // Collect holes in the instantiated lemma as new goals
    DoublyLinkedList *holes = list_holes(instantiated_lemma);

    return init_unification_result(instantiated_lemma, holes);
}

Expression *unification_result_get_lemma_instantiation(UnificationResult *result) {
    if (!result) {
        return NULL;
    }
    return result->lemma_instantiation;
}

DoublyLinkedList *unification_result_get_new_goals(UnificationResult *result) {
    if (!result) {
        return NULL;
    }
    return result->new_goals;
}
