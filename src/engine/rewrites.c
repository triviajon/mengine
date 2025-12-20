#include "rewrites.h"

bool nothing_rewritten(RewriteProof *rewrite_proof) {
    return rewrite_proof->expr == rewrite_proof->rewritten_expr;
}

bool rewrite_failed(RewriteProof *rewrite_proof) {
    return rewrite_proof == NULL;
}

RewriteProof *get_rresult(Expression *expr) {
    switch (expr->type) {
        case (APP_EXPRESSION): {
            return expr->value.app.rresult;
        }
        case (LAMBDA_EXPRESSION): {
            return expr->value.lambda.rresult;
        }
        case (VAR_EXPRESSION): {
            return expr->value.var.rresult;
        }
        default:
            return NULL;
    }
}

void set_rresult(Expression *expr, RewriteProof *rresult) {
    switch (expr->type) {
        case (APP_EXPRESSION): {
            expr->value.app.rresult = rresult;
            break;
        }
        case (LAMBDA_EXPRESSION): {
            expr->value.lambda.rresult = rresult;
            break;
        }
        case (VAR_EXPRESSION): {
            expr->value.var.rresult = rresult;
            break;
        }
        default:
            break;
    }
}

void clear_rewrite_proofs(Expression *expr) {
    // Need to double check for correctness.

    switch (expr->type) {
        case (APP_EXPRESSION): {
            if (expr->value.app.rresult == NULL) {
                break;
            }
            expr->value.app.rresult = NULL;
            clear_rewrite_proofs(expr->value.app.func);
            clear_rewrite_proofs(expr->value.app.arg);
            break;
        }
        case (LAMBDA_EXPRESSION): {
            if (expr->value.lambda.rresult == NULL) {
                break;
            }
            expr->value.lambda.rresult = NULL;
            clear_rewrite_proofs(expr->value.lambda.body);
            break;
        }
        case (VAR_EXPRESSION): {
            expr->value.var.rresult = NULL;
            break;
        }
        default:
            break;
    }
}

RewriteProof *rewrite_head(Context *goal_context, Expression *expr,
                           Expression *lemma) {
    Expression *lemma_ty = get_expression_type(lemma);

    if (lemma_ty->type == FORALL_EXPRESSION) {
        UnificationResult *unification_result =
            unify_and_instantiate(goal_context, lemma, lemma_ty, expr);
        if (unification_result == NULL)
            return init_rewrite_proof(expr, expr, build_eq_refl(expr),
                                      dll_create());

        Expression *instantiated_lemma =
            unification_result->lemma_instantiation;
        if (instantiated_lemma != NULL) {
            Expression *lhs =
                get_lhs_eq(get_expression_type(instantiated_lemma));
            Expression *rhs =
                get_rhs_eq(get_expression_type(instantiated_lemma));

            if (congruence(lhs, expr)) {
                return init_rewrite_proof(expr, rhs, instantiated_lemma,
                                          unification_result->new_goals);
            }
        }

        return init_rewrite_proof(expr, expr, build_eq_refl(expr),
                                  dll_create());
    }

    Expression *lhs = get_lhs_eq(lemma_ty);
    Expression *rhs = get_rhs_eq(lemma_ty);
    if (congruence(lhs, expr)) {
        return init_rewrite_proof(expr, rhs, lemma, dll_create());
    } else {
        return init_rewrite_proof(expr, expr, build_eq_refl(expr),
                                  dll_create());
    }
}

RewriteProof *rewrites_head(Context *goal_context, Expression *expr, int n,
                            va_list lemmas) {
    RewriteProof *current_proof =
        init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
    Expression *current_expr = expr;

    while (true) {
        bool rewrote = false;
        va_list copy;
        va_copy(copy, lemmas);
        for (int i = 0; i < n; i++) {
            Expression *ith_lemma = va_arg(copy, Expression *);
            Expression *ith_lemma_ty = get_expression_type(ith_lemma);

            UnificationResult *unification_result = unify_and_instantiate(
                goal_context, ith_lemma, ith_lemma_ty, current_expr);
            if (unification_result == NULL) {
                continue;
            }
            Expression *instantiated_lemma =
                unification_result->lemma_instantiation;

            if (instantiated_lemma != NULL) {
                Expression *lhs =
                    get_lhs_eq(get_expression_type(instantiated_lemma));
                Expression *rhs =
                    get_rhs_eq(get_expression_type(instantiated_lemma));
                if (congruence(lhs, current_expr)) {
                    RewriteProof *curr_to_next = init_rewrite_proof(
                        current_expr, rhs, instantiated_lemma,
                        unification_result->new_goals);
                    Expression *proof_of_curr_to_next =
                        build_eq_trans(current_proof, curr_to_next);
                    current_proof = init_rewrite_proof(
                        expr, rhs, proof_of_curr_to_next,
                        dll_merge(current_proof->remaining_goals,
                                  curr_to_next->remaining_goals));
                    current_expr = rhs;
                    free_rewrite_proof(curr_to_next);
                    rewrote = true;
                }
            }
        }
        va_end(copy);
        if (!rewrote) {
            return current_proof;
        }
    }
}

RewriteProof *rewrite_lambda(Context *goal_context, Expression *expr,
                             Expression *lemma) {
    Expression *x = expr->value.lambda.bound_variable;
    Expression *T = get_expression_type(x);
    Expression *inner_orig = expr->value.lambda.body;

    RewriteProof *inner_rw = _rewrite(goal_context, inner_orig, lemma);
    Expression *mid =
        refresh(init_lambda_expression(x, inner_rw->rewritten_expr));

    Expression *eq_pf_ty = get_expression_type(inner_rw->equality_proof);
    Expression *pre_func_ext =
        refresh(init_lambda_expression(x, inner_rw->equality_proof));

    Expression *A = T;
    Expression *B = get_expression_type(inner_orig);

    Expression *eq_ty_lhs =
        refresh(init_lambda_expression(x, get_lhs_eq(eq_pf_ty)));
    Expression *eq_ty_rhs =
        refresh(init_lambda_expression(x, get_rhs_eq(eq_pf_ty)));

    // @functional_extensionality : forall (A B : Type) (f g : A -> B), (forall
    // x : A, eq B (f x) (g x)) -> eq (A -> B) f g
    Expression *f_mid =
        build_lambda_extensionality(A, B, eq_ty_lhs, eq_ty_rhs, pre_func_ext);
    RewriteProof *rewritten_mid = rewrite_head(goal_context, mid, lemma);

    RewriteProof *result;
    if (nothing_rewritten(rewritten_mid)) {
        result =
            init_rewrite_proof(expr, mid, f_mid, inner_rw->remaining_goals);
    } else {
        result = init_rewrite_proof(
            expr, rewritten_mid->rewritten_expr,
            build_eq_trans(init_rewrite_proof(expr, mid, f_mid, dll_create()),
                           rewritten_mid),
            dll_merge(inner_rw->remaining_goals,
                      rewritten_mid->remaining_goals));
    }

    free_rewrite_proof(rewritten_mid);
    set_rresult(expr, result);
    return result;
}

RewriteProof *rewrites_lambda(Context *goal_context, Expression *expr, int n,
                              va_list lemmas) {
    Expression *x = expr->value.lambda.bound_variable;
    Expression *T = get_expression_type(x);
    Expression *inner_orig = expr->value.lambda.body;

    RewriteProof *inner_rw = _rewrites(goal_context, inner_orig, n, lemmas);
    Expression *mid =
        refresh(init_lambda_expression(x, inner_rw->rewritten_expr));

    Expression *eq_pf_ty = get_expression_type(inner_rw->equality_proof);
    Expression *pre_func_ext =
        refresh(init_lambda_expression(x, inner_rw->equality_proof));

    Expression *A = T;
    Expression *B = get_expression_type(inner_orig);

    Expression *eq_ty_lhs =
        refresh(init_lambda_expression(x, get_lhs_eq(eq_pf_ty)));
    Expression *eq_ty_rhs =
        refresh(init_lambda_expression(x, get_rhs_eq(eq_pf_ty)));

    // @functional_extensionality : forall (A B : Type) (f g : A -> B), (forall
    // x : A, eq B (f x) (g x)) -> eq (A -> B) f g
    Expression *f_mid =
        build_lambda_extensionality(A, B, eq_ty_lhs, eq_ty_rhs, pre_func_ext);
    RewriteProof *rewritten_mid = rewrites_head(goal_context, mid, n, lemmas);

    RewriteProof *result;
    if (nothing_rewritten(rewritten_mid)) {
        result = init_rewrite_proof(expr, mid, f_mid, dll_create());
    } else {
        result = init_rewrite_proof(
            expr, rewritten_mid->rewritten_expr,
            build_eq_trans(init_rewrite_proof(expr, mid, f_mid, dll_create()),
                           rewritten_mid),
            dll_create());
    }

    free_rewrite_proof(rewritten_mid);
    set_rresult(expr, result);
    return result;
}

RewriteProof *rewrite_app(Context *goal_context, Expression *expr,
                          Expression *lemma) {
    Expression *func = expr->value.app.func;
    Expression *arg = expr->value.app.arg;
    RewriteProof *rw_func_proof = _rewrite(goal_context, func, lemma);
    RewriteProof *rw_arg_proof = _rewrite(goal_context, arg, lemma);

    RewriteProof *mid_rewrite_proof;

    if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
        mid_rewrite_proof =
            init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
    } else {
        DoublyLinkedList *merged = dll_merge(rw_func_proof->remaining_goals,
                                             rw_arg_proof->remaining_goals);
        mid_rewrite_proof = init_rewrite_proof(
            expr,
            init_app_expression(rw_func_proof->rewritten_expr,
                                rw_arg_proof->rewritten_expr),
            build_app_cong(rw_func_proof, rw_arg_proof), merged);
    }

    Expression *mid = mid_rewrite_proof->rewritten_expr;
    Expression *fx_mid = mid_rewrite_proof->equality_proof;
    DoublyLinkedList *mid_goals = mid_rewrite_proof->remaining_goals;

    RewriteProof *rewritten_mid = rewrite_head(goal_context, mid, lemma);
    RewriteProof *result;
    if (nothing_rewritten(rewritten_mid)) {
        result = init_rewrite_proof(expr, mid, fx_mid, mid_goals);
    } else {
        DoublyLinkedList *merged =
            dll_merge(mid_goals, rewritten_mid->remaining_goals);
        result = init_rewrite_proof(
            expr, rewritten_mid->rewritten_expr,
            build_eq_trans(mid_rewrite_proof, rewritten_mid), merged);
    }

    free_rewrite_proof(mid_rewrite_proof);
    free_rewrite_proof(rewritten_mid);

    set_rresult(expr, result);
    return result;
}

RewriteProof *rewrites_app(Context *goal_context, Expression *expr, int n,
                           va_list lemmas) {
    Expression *func = expr->value.app.func;
    Expression *arg = expr->value.app.arg;
    RewriteProof *rw_func_proof = _rewrites(goal_context, func, n, lemmas);
    RewriteProof *rw_arg_proof = _rewrites(goal_context, arg, n, lemmas);

    RewriteProof *mid_rewrite_proof;

    if (nothing_rewritten(rw_func_proof) && nothing_rewritten(rw_arg_proof)) {
        mid_rewrite_proof =
            init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
    } else {
        DoublyLinkedList *merged = dll_merge(rw_func_proof->remaining_goals,
                                             rw_arg_proof->remaining_goals);
        mid_rewrite_proof = init_rewrite_proof(
            expr,
            init_app_expression(rw_func_proof->rewritten_expr,
                                rw_arg_proof->rewritten_expr),
            build_app_cong(rw_func_proof, rw_arg_proof), merged);
    }

    Expression *mid = mid_rewrite_proof->rewritten_expr;
    Expression *fx_mid = mid_rewrite_proof->equality_proof;
    DoublyLinkedList *mid_goals = mid_rewrite_proof->remaining_goals;

    RewriteProof *rewritten_mid = rewrites_head(goal_context, mid, n, lemmas);
    RewriteProof *result;
    if (nothing_rewritten(rewritten_mid)) {
        result = init_rewrite_proof(expr, mid, fx_mid, mid_goals);
    } else {
        DoublyLinkedList *merged =
            dll_merge(mid_goals, rewritten_mid->remaining_goals);
        result = init_rewrite_proof(
            expr, rewritten_mid->rewritten_expr,
            build_eq_trans(mid_rewrite_proof, rewritten_mid), merged);
    }

    free_rewrite_proof(mid_rewrite_proof);
    free_rewrite_proof(rewritten_mid);

    set_rresult(expr, result);
    return result;
}

RewriteProof *rewrite_var(Context *goal_context, Expression *expr,
                          Expression *lemma) {
    RewriteProof *rewritten_expr = rewrite_head(goal_context, expr, lemma);
    if (nothing_rewritten(rewritten_expr)) {
        RewriteProof *result =
            init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
        set_rresult(expr, result);
        return result;
    } else {
        set_rresult(expr, rewritten_expr);
        return rewritten_expr;
    }
}

RewriteProof *rewrites_var(Context *goal_context, Expression *expr, int n,
                           va_list lemmas) {
    RewriteProof *rewritten_expr = rewrites_head(goal_context, expr, n, lemmas);
    if (nothing_rewritten(rewritten_expr)) {
        RewriteProof *result =
            init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
        set_rresult(expr, result);
        return result;
    } else {
        set_rresult(expr, rewritten_expr);
        return rewritten_expr;
    }
}

RewriteProof *rewrite_hole(Context *goal_context, Expression *expr) {
    (void)goal_context;  // Unused in this function
    return init_rewrite_proof(expr, expr, build_eq_refl(expr), dll_create());
}

// Internal rewriting function.
RewriteProof *_rewrite(Context *goal_context, Expression *expr,
                       Expression *lemma) {
    RewriteProof *cached_result = get_rresult(expr);
    if (cached_result != NULL && cached_result->expr != NULL) {
        return cached_result;
    }

    switch (expr->type) {
        case (APP_EXPRESSION):
            return rewrite_app(goal_context, expr, lemma);
        case (LAMBDA_EXPRESSION):
            return rewrite_lambda(goal_context, expr, lemma);
        case (VAR_EXPRESSION):
            return rewrite_var(goal_context, expr, lemma);
        case (HOLE_EXPRESSION):
            return rewrite_hole(goal_context, expr);
        default:
            return NULL;  // TODO: Unsupported.
    }
}

void print_ptr_counts(void) {
    if (ptr_counter == NULL) {
        return;
    }

    for (int i = 0; i < ptr_counter->size; i++) {
        void *key = (ptr_counter->items + i)->key;
        int *count = (int *)((ptr_counter->items + i)->val);
        if (*count > 0) {
            printf("Pointer %p: %d times [%s]\n", key, *count, se(key));
        }
    }
}

RewriteProof *_rewrites(Context *goal_context, Expression *expr, int n,
                        va_list lemmas) {
    if (ptr_counter == NULL) {
        ptr_counter = map_new();
    }
    // int *count = map_get(ptr_counter, expr);
    // if (count == NULL) {
    //   count = calloc(1, sizeof(int));
    //   map_set(ptr_counter, expr, count);
    // }
    // (*count)++;

    RewriteProof *cached_result = get_rresult(expr);
    if (cached_result != NULL) {
        return cached_result;
    }

    switch (expr->type) {
        case (APP_EXPRESSION):
            return rewrites_app(goal_context, expr, n, lemmas);
        case (LAMBDA_EXPRESSION):
            return rewrites_lambda(goal_context, expr, n, lemmas);
        case (VAR_EXPRESSION):
            return rewrites_var(goal_context, expr, n, lemmas);
        case (HOLE_EXPRESSION):
            return rewrite_hole(goal_context, expr);
        default:
            return NULL;  // TODO: Unsupported.
    }
}

// Top level rewriting function. Is responsible for clearing the cached
// RewriteProof results of Expressions.
// The reason this function calls for a goal context is that usually, expr is
// the expected return type of a hole, and we are working to rewrite the return
// type itself so that we can more easily find an inhabitant of the type. In the
// case where rewrite generates more goals so solve, those new goals should have
// the same context as the original goal context. In an ideal world, TODO:,
// reimplement rewrite to operate given a hole instead of a type.
RewriteProof *rewrite(Context *goal_context, Expression *expr,
                      Expression *lemma) {
    RewriteProof *result = NULL;
    switch (expr->type) {
        case (APP_EXPRESSION):
            result = rewrite_app(goal_context, expr, lemma);
            break;
        case (LAMBDA_EXPRESSION):
            result = rewrite_lambda(goal_context, expr, lemma);
            break;
        case (VAR_EXPRESSION):
            result = rewrite_var(goal_context, expr, lemma);
            break;
        default:
            return NULL;  // TODO: Unsupported.
    }

    clear_rewrite_proofs(expr);
    return result;
}

RewriteProof *rewrites(Context *goal_context, Expression *expr, int n,
                       va_list lemmas) {
    RewriteProof *result = NULL;
    switch (expr->type) {
        case (APP_EXPRESSION):
            result = rewrites_app(goal_context, expr, n, lemmas);
            break;
        case (LAMBDA_EXPRESSION):
            result = rewrites_lambda(goal_context, expr, n, lemmas);
            break;
        case (VAR_EXPRESSION):
            result = rewrites_var(goal_context, expr, n, lemmas);
            break;
        default:
            return NULL;  // TODO: Unsupported.
    }

    clear_rewrite_proofs(expr);
    return result;
}

// Given a goal (a hole with an expected return type) and a lemma,
// this function attempts to transform the goal's return type according to the
// lemma. I.e., on a successful rewrite of the goal's type, this function will
// return two things: 	1) A new hole, representing the new goal to find a term
// which has it's return type.
//  2) A proof term that satisfies the original goal. This proof term will have
//  the above mentioned hole,
//     which will need to be filled.
RewrittenGoal *rewrite_transform(Expression *goal, Expression *rewrite_lemma) {
    Expression *goal_type = get_expression_type(goal);
    RewriteProof *rewrite_proof =
        rewrite(get_expression_context(goal), goal_type, rewrite_lemma);

    Expression *new_goal = init_hole_expression(
        "Goal", rewrite_proof->rewritten_expr, get_expression_context(goal));
    Expression *proof = init_app_expression(
        init_app_expression(
            init_app_expression(
                init_app_expression(eq_subst, rewrite_proof->expr),
                rewrite_proof->rewritten_expr),
            rewrite_proof->equality_proof),
        new_goal);

    fill_hole(goal, proof);

    return init_rewritten_goal(new_goal, rewrite_proof->remaining_goals);
}

// Similar to rewrite_transform, but accepts an arbitrary number of lemmas and
// applies each of them sequentially, forever, until the goal is no longer
// transformed by any of the lemmas.
RewrittenGoal *rewrites_transform(Expression *goal, int n, ...) {
    va_list args;
    DoublyLinkedList *remaining_open = dll_create();

    Expression *curr_goal = goal;
    while (true) {
        va_start(args, n);

        Expression *curr_goal_ty = get_expression_type(curr_goal);
        RewriteProof *rewrite_proof =
            rewrites(get_expression_context(goal), curr_goal_ty, n, args);

        va_end(args);
        if (nothing_rewritten(rewrite_proof)) {
            break;
        }

        Expression *new_goal =
            init_hole_expression("Goal", rewrite_proof->rewritten_expr,
                                 get_expression_context(goal));
        Expression *proof = init_app_expression(
            init_app_expression(
                init_app_expression(
                    init_app_expression(eq_subst, rewrite_proof->expr),
                    rewrite_proof->rewritten_expr),
                rewrite_proof->equality_proof),
            new_goal);

        fill_hole(curr_goal, proof);
        remaining_open =
            dll_merge(remaining_open, rewrite_proof->remaining_goals);
        curr_goal = new_goal;
    }

    return init_rewritten_goal(curr_goal, remaining_open);
}
