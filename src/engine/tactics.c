#include "src/engine/tactics.h"

#include <stdlib.h>

#include "src/engine/unify.h"
#include "src/kernel/doubly_linked_list.h"
#include "src/kernel/expression.h"
#include "src/kernel/new_subst.h"
#include "src/runtime/core.h"

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals,
                                 char *error_message) {
    TacticResult *result = malloc(sizeof(TacticResult));
    if (!result) {
        return NULL;
    }

    result->success = success;
    result->new_goals = new_goals;
    result->error_message = error_message;

    return result;
}

void free_tactic_result(TacticResult *result) {
    if (!result) {
        return;
    }
    free(result);
}

// Helper that performs a single intro step, returning the new goal on success
// or NULL on failure.
static Expression *intro_step(Expression *goal, char *name, char **error_out) {
    if (goal->type != HOLE_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a hole";
        }
        return NULL;
    }

    Expression *goal_ty = get_expression_type(goal);
    if (goal_ty->type != FORALL_EXPRESSION) {
        if (error_out) {
            *error_out = "Goal is not a forall expression";
        }
        return NULL;
    }

    Expression *x = goal_ty->value.forall.bound_variable;
    Expression *A = get_expression_type(x);
    Expression *B = goal_ty->value.forall.body;

    Expression *x_prime =
        init_var_expression_wc(name, A, get_expression_context(goal));
    Expression *B_prime = new_subst(B, x, x_prime);
    Context *new_context =
        context_insert(get_expression_context(goal), x_prime);
    Expression *new_goal = init_hole_expression("Goal", B_prime, new_context);

    Expression *proof_of_original =
        init_lambda_expression_wc(x_prime, new_goal, new_context);
    if (can_fill(goal, proof_of_original)) {
        fill_hole(goal, proof_of_original);
        return new_goal;
    }

    if (error_out) {
        *error_out = "Failed to fill the hole";
    }
    return NULL;
}

TacticResult *intro_tactic(Expression *goal, char *name) {
    char *error = NULL;
    Expression *new_goal = intro_step(goal, name, &error);

    if (!new_goal) {
        return init_tactic_result(false, NULL, error);
    }

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    return init_tactic_result(true, new_goals, NULL);
}

TacticResult *intros_tactic(Expression *goal, char **names, size_t name_count) {
    if (name_count == 0) {
        return init_tactic_result(false, NULL,
                                  "intros requires at least one name");
    }

    Expression *current_goal = goal;
    char *error = NULL;

    for (size_t i = 0; i < name_count; i++) {
        Expression *next_goal = intro_step(current_goal, names[i], &error);
        if (!next_goal) {
            return init_tactic_result(false, NULL, error);
        }
        current_goal = next_goal;
    }

    // Return only the final goal
    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(current_goal));
    return init_tactic_result(true, new_goals, NULL);
}

TacticResult *apply_tactic(Expression *goal, Expression *lemma) {
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL,
                                  "Could not unify lemma with goal");
    }

    // For apply, we do not allow the unification result to have any remaining
    // open holes.
    if (dll_len(unif_result->new_goals) != 0) {
        free_unification_result(unif_result);
        return init_tactic_result(
            false, NULL, "Apply tactic does not allow remaining open holes");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!can_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL,
                                  "Cannot fill goal with lemma instantiation");
    }

    fill_hole(goal, lemma_inst);

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    free_unification_result(unif_result);
    return result;
}

TacticResult *eapply_tactic(Expression *goal, Expression *lemma) {
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    // Attempt to unify the lemma with the goal
    UnificationResult *unif_result = eunify2(lemma, goal);
    if (!unif_result) {
        return init_tactic_result(false, NULL,
                                  "Could not unify lemma with goal");
    }

    // Check if the unification succeeded by verifying types match
    Expression *lemma_inst = unif_result->lemma_instantiation;

    if (!can_fill(goal, lemma_inst)) {
        free_unification_result(unif_result);
        return init_tactic_result(false, NULL,
                                  "Cannot fill goal with lemma instantiation");
    }

    fill_hole(goal, lemma_inst);

    DoublyLinkedList *new_goals = unif_result->new_goals;
    TacticResult *result = init_tactic_result(true, new_goals, NULL);

    free(unif_result);

    return result;
}

TacticResult *assumption_tactic(Expression *goal) {
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    Context *ctx = get_expression_context(goal);

    // Iterate through the context to find a variable whose type matches the
    // goal
    while (ctx && !context_is_empty(ctx)) {
        Expression *var = ctx->var_type;

        if (can_fill(goal, var)) {
            fill_hole(goal, var);
            return init_tactic_result(true, dll_create(), NULL);
        }

        ctx = ctx->parent;
    }

    return init_tactic_result(false, NULL, "No assumption matches the goal");
}

TacticResult *exact_tactic(Expression *goal, Expression *proof_term) {
    if (goal->type != HOLE_EXPRESSION) {
        return init_tactic_result(false, NULL, "Goal is not a hole");
    }

    if (!can_fill(goal, proof_term)) {
        return init_tactic_result(false, NULL,
                                  "Cannot fill goal with proof term");
    }

    fill_hole(goal, proof_term);
    return init_tactic_result(true, dll_create(), NULL);
}

// === Rewriting section ===
// Forward declaration
RewriteResult *n_rewrite(Expression *expr, Expression *lemma, Context *context);

bool n_rewrite_is_noop(RewriteResult *rwr) {
    return rwr->original == rwr->rewritten;
}

Expression *_build_reflexivity_proof(Expression *expr, Context *ctx) {
    // The goal is to build a proof of eq type(expr) expr expr.

    Expression *relation_over = get_expression_type(expr);
    Expression *proof = init_app_expression_wc(
        init_app_expression_wc(eq_refl, relation_over, ctx), expr, ctx);

    return proof;
}

Expression *_build_transitivity_proof(RewriteResult *first_rwr,
                                      RewriteResult *second_rwr, Context *ctx) {
    // Given first_rwr : original -> mid with proof pf
    // and second_rwr : mid -> rewritten with proof pg
    // eq_trans : forall (A : Type) (x y z : A), eq A x y -> eq A y z -> eq A x
    // z. build the term "eq_trans relation_over original mid rewritten H1 H2"
    Expression *relation_over = get_expression_type(first_rwr->original);
    Expression *original = first_rwr->original;
    Expression *mid = first_rwr->rewritten;
    Expression *rewritten = second_rwr->rewritten;
    Expression *H1 = first_rwr->original_to_rewritten_proof;
    Expression *H2 = second_rwr->original_to_rewritten_proof;

    Expression *proof = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(eq_trans, relation_over, ctx),
                        original, ctx),
                    mid, ctx),
                rewritten, ctx),
            H1, ctx),
        H2, ctx);

    return proof;
}

Expression *_build_app_congruence_proof(RewriteResult *func_rwr,
                                        RewriteResult *arg_rwr, Context *ctx) {
    // Bad_App_Congruence : forall (A B : Type) (f g : A -> B) (x y: A), eq (A
    // -> B) f g -> eq (A) x y -> eq (B) (f x) (g y). if func_rw provides pf :
    // eq (A -> B) f g and arg_rwr provided pg : eq A x y, build the term
    // "Bad_App_Congruence A B f g x y pf pg"

    Expression *A_implies_B = get_app_arg(get_app_func(get_app_func(
        get_expression_type(func_rwr->original_to_rewritten_proof))));

    Expression *A = get_arrow_lhs(A_implies_B);
    Expression *B = get_arrow_rhs(A_implies_B);

    Expression *f = func_rwr->original;
    Expression *g = func_rwr->rewritten;

    Expression *x = arg_rwr->original;
    Expression *y = arg_rwr->rewritten;

    Expression *H1 = func_rwr->original_to_rewritten_proof;
    Expression *H2 = arg_rwr->original_to_rewritten_proof;

    Expression *proof = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(
                            init_app_expression_wc(
                                init_app_expression_wc(Bad_App_Congruence, A,
                                                       ctx),
                                B, ctx),
                            f, ctx),
                        g, ctx),
                    x, ctx),
                y, ctx),
            H1, ctx),
        H2, ctx);

    return proof;
}

RewriteResult *init_rewrite_result(Expression *original, Expression *rewritten,
                                   DoublyLinkedList *new_goals,
                                   Expression *original_to_rewritten_proof) {
    RewriteResult *rwr = malloc(sizeof(RewriteResult));
    if (!rwr) {
        return NULL;
    }

    rwr->original = original;
    rwr->rewritten = rewritten;
    rwr->new_goals = new_goals;
    rwr->original_to_rewritten_proof = original_to_rewritten_proof;

    return rwr;
}

void free_rewrite_result(RewriteResult *rwr) {
    if (!rwr) {
        return;
    }
    free(rwr);
}

RewriteResult *n_rewrite_head(Expression *mid, Expression *lemma,
                              Context *context) {
    // This part of rewriting is literally just a call to the apply tactic.
    // We're creating a hole with expected return type `mid` and defining
    // context `context`, and attempting to apply the lemma to it. Expression
    // *hole = init_hole_expression("Rewrite_Hole", mid, context);

    // // TODO: Temporary, until the hole fill bug is fixed.
    // Expression *holder =
    // init_lambda_expression_wc(init_var_expression_wc("t",
    // init_type_expression(), context),
    //      hole, context);

    // TODO: We shouldn't need to use this function
    UnificationResult *unif_result = bad_unify_for_eq(context, lemma, mid);

    if (!unif_result) {
        return init_rewrite_result(mid, mid, dll_create(),
                                   _build_reflexivity_proof(mid, context));
    }

    // For rewriting, we do not allow the unification result to have any
    // remaining open holes.
    if (dll_len(unif_result->new_goals) > 0) {
        free_unification_result(unif_result);
        return init_rewrite_result(mid, mid, dll_create(),
                                   _build_reflexivity_proof(mid, context));
    }

    Expression *proof = unif_result->lemma_instantiation;
    Expression *proof_type = get_expression_type(proof);
    if (!congruence(_get_lhs_eq(proof_type), mid)) {
        free_unification_result(unif_result);
        return init_rewrite_result(mid, mid, dll_create(),
                                   _build_reflexivity_proof(mid, context));
    }

    free_unification_result(unif_result);

    return init_rewrite_result(mid, _get_rhs_eq(proof_type), dll_create(),
                               proof);
}

RewriteResult *n_rewrite_app(Expression *expr, Expression *lemma,
                             Context *context) {
    Expression *func = get_app_func(expr);
    Expression *arg = get_app_arg(expr);

    RewriteResult *rwr_func = n_rewrite(func, lemma, context);
    RewriteResult *rwr_arg = n_rewrite(arg, lemma, context);

    RewriteResult *mid_rwr = NULL;
    if (n_rewrite_is_noop(rwr_func) && n_rewrite_is_noop(rwr_arg)) {
        mid_rwr = init_rewrite_result(expr, expr, dll_create(),
                                      _build_reflexivity_proof(expr, context));
    } else {
        mid_rwr = init_rewrite_result(
            expr,
            init_app_expression_wc(rwr_func->rewritten, rwr_arg->rewritten,
                                   context),
            dll_merge(rwr_func->new_goals, rwr_arg->new_goals),
            _build_app_congruence_proof(rwr_func, rwr_arg, context));
    }

    RewriteResult *mid_result =
        n_rewrite_head(mid_rwr->rewritten, lemma, context);

    RewriteResult *final_rwr;
    if (n_rewrite_is_noop(mid_result)) {
        final_rwr =
            init_rewrite_result(expr, mid_rwr->rewritten, mid_rwr->new_goals,
                                mid_rwr->original_to_rewritten_proof);
    } else {
        final_rwr = init_rewrite_result(
            expr, mid_result->rewritten,
            dll_merge(mid_rwr->new_goals, mid_result->new_goals),
            _build_transitivity_proof(mid_rwr, mid_result, context));
    }

    free_rewrite_result(rwr_func);
    free_rewrite_result(rwr_arg);
    free_rewrite_result(mid_rwr);
    free_rewrite_result(mid_result);

    return final_rwr;
}

RewriteResult *n_rewrite_var(Expression *expr, Expression *lemma,
                             Context *context) {
    RewriteResult *head_rwr = n_rewrite_head(expr, lemma, context);
    return head_rwr;
}

RewriteResult *n_rewrite(Expression *expr, Expression *lemma,
                         Context *context) {
    RewriteResult *result = NULL;
    switch (expr->type) {
        case (APP_EXPRESSION): {
            result = n_rewrite_app(expr, lemma, context);
            break;
        }
        case (VAR_EXPRESSION): {
            result = n_rewrite_var(expr, lemma, context);
            break;
        }
        default:
            return NULL;
    }
    return result;
}

TacticResult *rewrite_tactic(Expression *goal, Expression *lemma) {
    // Assume return_type has form R lhs rhs, so that R is the relation and the
    // type of lhs/rhs are what the relation are over.
    Expression *return_type = get_expression_type(goal);
    Context *operating_ctx = get_expression_context(goal);

    Expression *func1 = get_app_func(return_type);
    if (!func1) {
        return init_tactic_result(false, NULL,
                                  "Goal type is not an application");
    }
    Expression *relation_left_hand = get_app_arg(func1);
    Expression *relation_right_hand = get_app_arg(return_type);
    if (!relation_left_hand || !relation_right_hand) {
        return init_tactic_result(false, NULL, "Goal type malformed");
    }
    Expression *func2 = get_app_func(func1);
    if (!func2) {
        return init_tactic_result(false, NULL,
                                  "Goal type is not a binary relation");
    }
    Expression *relation = func2;
    Expression *relation_over = get_expression_type(relation_right_hand);

    // Require that Equivalence proof applies to relation
    // TODO: Since we are hardcoding rewriting only for Leibniz Equality....
    if (get_app_func(relation) != eq) {
        return init_tactic_result(
            false, NULL,
            "Currently only rewriting for Leibniz Equality is supported");
    }

    // Once that's confirmed, we can begin attempting to rewrite. Start with the
    // lhs and try to apply the lemma.
    RewriteResult *rwr = n_rewrite(relation_left_hand, lemma, operating_ctx);
    if (!rwr) {
        return init_tactic_result(false, NULL, "Rewriting failed");
    }

    // rwr gives us eq A lhs lhs' with proof pf. We now need to build the proof
    // of eq relation_over lhs rhs. We'll just use eq_trans : (forall (A: Type),
    // (forall (x: A), (forall (y: A), (forall (z: A), (forall (_: (((eq A) x)
    // y)), (forall (_: (((eq A) y) z)), (((eq A) x) z)))))))
    Expression *new_goal_type = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(eq, relation_over, operating_ctx),
            rwr->rewritten, operating_ctx),
        relation_right_hand, operating_ctx);
    Expression *new_goal =
        init_hole_expression("Goal", new_goal_type, operating_ctx);
    Expression *proof_of_goal = init_app_expression_wc(
        init_app_expression_wc(
            init_app_expression_wc(
                init_app_expression_wc(
                    init_app_expression_wc(
                        init_app_expression_wc(eq_trans, relation_over,
                                               operating_ctx),
                        rwr->original, operating_ctx),
                    rwr->rewritten, operating_ctx),
                relation_right_hand, operating_ctx),
            rwr->original_to_rewritten_proof, operating_ctx),
        new_goal, operating_ctx);

    DoublyLinkedList *new_goals = dll_create();
    dll_insert_at_tail(new_goals, dll_new_node(new_goal));
    new_goals = dll_merge(new_goals, rwr->new_goals);

    if (!can_fill(goal, proof_of_goal)) {
        free_rewrite_result(rwr);
        return init_tactic_result(false, NULL,
                                  "Failed to fill the goal after rewriting");
    }

    fill_hole(goal, proof_of_goal);

    TacticResult *tac_result = init_tactic_result(true, new_goals, NULL);
    free_rewrite_result(rwr);
    return tac_result;
}