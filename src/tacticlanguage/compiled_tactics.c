/*
 * compiled_tactics.c — JIT-style compiled replacements for scripted tactics.
 *
 * tactic_def_attach_compiled() is called after each tactic definition is
 * parsed.  It inspects the tactic body and, when it recognises a known
 * pattern AND all required axioms are in scope, replaces def->fn with a C
 * implementation that constructs the same proof terms without going through
 * the tactic interpreter.
 *
 * Currently compiled tactics:
 *   compiled_sep_cancel  — replaces `cancel` when its body is exactly
 *       `repeat __cancel_one; try reflexivity` and {sep, sep_comm,
 *       sep_swap, sep_cong_r} are in scope.  Implements the same
 *       bring-to-front + strip loop as the scripted __cancel_one /__
 *       bring_to_front reference definitions (see separation_logic.py).
 *       The scripted definitions are still parsed (and serve as a
 *       readable spec and interpreter fallback), but are NOT called
 *       during normal execution when this compiled version is active.
 */
#include "src/tacticlanguage/compiled_tactics.h"

#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/runtime/core.h"

typedef struct {
    Expression *sep;
    Expression *sep_comm;
    Expression *sep_swap;
    Expression *sep_cong_r;
} SepCancelEnv;

typedef struct {
    Expression *rewritten;
    Expression *proof;
} BringToFrontResult;

static Expression *ctx_lookup(Context *ctx, const char *name) {
    return kernel_context_lookup(ctx, (char *)name);
}

static Expression *app(Expression *f, Expression *x, Context *ctx) {
    if (!f || !x) {
        return NULL;
    }
    return kernel_app_create(f, x, ctx);
}

static Expression *app2(Expression *f, Expression *a, Expression *b, Context *ctx) {
    return app(app(f, a, ctx), b, ctx);
}

static Expression *app3(Expression *f, Expression *a, Expression *b, Expression *c, Context *ctx) {
    return app(app2(f, a, b, ctx), c, ctx);
}

static Expression *app4(Expression *f, Expression *a, Expression *b, Expression *c, Expression *d,
                        Context *ctx) {
    return app(app3(f, a, b, c, ctx), d, ctx);
}

static Expression *app6(Expression *f, Expression *a, Expression *b, Expression *c, Expression *d,
                        Expression *e, Expression *g, Context *ctx) {
    return app(app(app4(f, a, b, c, d, ctx), e, ctx), g, ctx);
}

static bool app_parts(Expression *expr, Expression **func, Expression **arg) {
    if (!expr || !kernel_expr_is_app(expr)) {
        return false;
    }
    *func = kernel_app_func(expr);
    *arg = kernel_app_arg(expr);
    return *func && *arg;
}

static bool sep_app_parts(SepCancelEnv *env, Expression *expr, Expression **head,
                          Expression **rest) {
    Expression *func = NULL;
    if (!app_parts(expr, &func, rest)) {
        return false;
    }
    Expression *sep_head = NULL;
    if (!app_parts(func, &sep_head, head)) {
        return false;
    }
    return sep_head == env->sep;
}

static bool eq_goal_parts(Expression *goal_ty, Expression **A, Expression **lhs, Expression **rhs) {
    Expression *eq_A_lhs = NULL;
    if (!app_parts(goal_ty, &eq_A_lhs, rhs)) {
        return false;
    }
    Expression *eq_A = NULL;
    if (!app_parts(eq_A_lhs, &eq_A, lhs)) {
        return false;
    }
    Expression *eq_head = NULL;
    if (!app_parts(eq_A, &eq_head, A)) {
        return false;
    }
    return eq_head == eq;
}

static Expression *build_eq_refl(Expression *expr, Context *ctx) {
    return app2(eq_refl, kernel_expr_type(expr), expr, ctx);
}

static Expression *build_eq_trans(Expression *A, Expression *from, Expression *mid, Expression *to,
                                  Expression *p1, Expression *p2, Context *ctx) {
    return app6(eq_trans, A, from, mid, to, p1, p2, ctx);
}

static bool bring_to_front(SepCancelEnv *env, Expression *target, Expression *lhs, Context *ctx,
                           BringToFrontResult *out) {
    if (lhs == target) {
        out->rewritten = lhs;
        out->proof = build_eq_refl(lhs, ctx);
        return out->proof != NULL;
    }

    Expression *lhs_head = NULL;
    Expression *lhs_rest = NULL;
    if (!sep_app_parts(env, lhs, &lhs_head, &lhs_rest)) {
        return false;
    }

    if (lhs_head == target) {
        out->rewritten = lhs;
        out->proof = build_eq_refl(lhs, ctx);
        return out->proof != NULL;
    }

    BringToFrontResult sub = {0};
    if (!bring_to_front(env, target, lhs_rest, ctx, &sub)) {
        return false;
    }

    Expression *T = kernel_expr_type(lhs);
    if (sub.rewritten == target) {
        Expression *mid = app2(env->sep, lhs_head, target, ctx);
        Expression *cong = app4(env->sep_cong_r, lhs_head, lhs_rest, target, sub.proof, ctx);
        Expression *swapped = app2(env->sep, target, lhs_head, ctx);
        Expression *swap = app2(env->sep_comm, lhs_head, target, ctx);
        Expression *trans = build_eq_trans(T, lhs, mid, swapped, cong, swap, ctx);
        if (!mid || !cong || !swapped || !swap || !trans) {
            return false;
        }
        out->rewritten = swapped;
        out->proof = trans;
        return true;
    }

    Expression *rest_head = NULL;
    Expression *rest2 = NULL;
    if (!sep_app_parts(env, sub.rewritten, &rest_head, &rest2)) {
        return false;
    }
    (void)rest_head;

    Expression *mid = app2(env->sep, lhs_head, sub.rewritten, ctx);
    Expression *cong = app4(env->sep_cong_r, lhs_head, lhs_rest, sub.rewritten, sub.proof, ctx);
    Expression *inner = app2(env->sep, lhs_head, rest2, ctx);
    Expression *swapped = app2(env->sep, target, inner, ctx);
    Expression *swap = app3(env->sep_swap, lhs_head, target, rest2, ctx);
    Expression *trans = build_eq_trans(T, lhs, mid, swapped, cong, swap, ctx);
    if (!mid || !cong || !inner || !swapped || !swap || !trans) {
        return false;
    }

    out->rewritten = swapped;
    out->proof = trans;
    return true;
}

static TacticResult *compiled_sep_cancel(MEngineRuntime *rt, Expression *goal, void *venv) {
    if (!rt || !goal || !venv || !kernel_expr_is_hole(goal)) {
        return engine_tactic_result_new(false, NULL, "compiled cancel: invalid goal");
    }

    SepCancelEnv *env = (SepCancelEnv *)venv;
    Context *ctx = kernel_expr_context(goal);
    Expression *current = goal;

    for (;;) {
        Expression *A = NULL;
        Expression *lhs = NULL;
        Expression *rhs = NULL;
        Expression *target = NULL;
        Expression *rest_r = NULL;
        bool cancellable = eq_goal_parts(kernel_expr_type(current), &A, &lhs, &rhs) &&
                           sep_app_parts(env, rhs, &target, &rest_r);
        if (!cancellable) {
            break;
        }

        BringToFrontResult rot = {0};
        if (!bring_to_front(env, target, lhs, ctx, &rot)) {
            break;
        }

        Expression *lhs_prime_head = NULL;
        Expression *lhs_prime_rest = NULL;
        if (!sep_app_parts(env, rot.rewritten, &lhs_prime_head, &lhs_prime_rest)) {
            break;
        }
        (void)lhs_prime_head;

        Expression *new_goal_ty = app3(eq, A, lhs_prime_rest, rest_r, ctx);
        Expression *new_goal = kernel_hole_create((char *)"Goal", new_goal_ty, ctx);
        Expression *strip = app4(env->sep_cong_r, target, lhs_prime_rest, rest_r, new_goal, ctx);
        Expression *rhs_target = app2(env->sep, target, rest_r, ctx);
        Expression *full = build_eq_trans(A, lhs, rot.rewritten, rhs_target, rot.proof, strip, ctx);

        if (!new_goal_ty || !new_goal || !strip || !rhs_target || !full ||
            !kernel_hole_fill(current, full)) {
            return engine_tactic_result_new(false, NULL, "compiled cancel: failed to fill goal");
        }

        if (current != goal) {
            kernel_free_filled_hole(current);
        }
        current = new_goal;
    }

    TacticResult *refl = engine_tactic_reflexivity(current);
    if (engine_tactic_result_success(refl)) {
        engine_tactic_result_free(refl);
        if (current != goal) {
            kernel_free_filled_hole(current);
        }
        return engine_tactic_result_new(true, dll_create(), NULL);
    }
    engine_tactic_result_free(refl);

    DoublyLinkedList *goals = dll_create();
    dll_insert_at_tail(goals, dll_new_node(current));
    return engine_tactic_result_new(true, goals, NULL);
}

static bool is_call0(TacticExpr *expr, const char *name) {
    return expr && expr->tag == TAC_CALL && expr->as.call.arg_count == 0 &&
           strcmp(expr->as.call.name, name) == 0;
}

/* Returns true iff body has the shape `repeat __cancel_one; try reflexivity`. */
static bool is_scripted_sep_cancel(TacticExpr *body) {
    if (!body || body->tag != TAC_SEQ) {
        return false;
    }
    TacticExpr *left = body->as.seq.left;
    TacticExpr *right = body->as.seq.right;
    return left && left->tag == TAC_REPEAT && is_call0(left->as.repeat.body, "__cancel_one") &&
           right && right->tag == TAC_TRY && is_call0(right->as.try_expr.body, "reflexivity");
}

/* If def matches a known compiled pattern, replace its fn pointer in-place. */
void tactic_def_attach_compiled(MEngineRuntime *rt, TacticDef *def) {
    if (!rt || !def || strcmp(def->name, "cancel") != 0 || def->param_count != 0 ||
        !is_scripted_sep_cancel(def->body)) {
        return;
    }

    SepCancelEnv *env = malloc(sizeof(SepCancelEnv));
    if (!env) {
        return;
    }
    env->sep = ctx_lookup(rt->ctx, "sep");
    env->sep_comm = ctx_lookup(rt->ctx, "sep_comm");
    env->sep_swap = ctx_lookup(rt->ctx, "sep_swap");
    env->sep_cong_r = ctx_lookup(rt->ctx, "sep_cong_r");
    if (!env->sep || !env->sep_comm || !env->sep_swap || !env->sep_cong_r) {
        free(env);
        return;
    }

    def->fn = compiled_sep_cancel;
    def->compiled_env = env;
}
