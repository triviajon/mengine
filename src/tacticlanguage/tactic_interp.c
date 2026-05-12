#include "src/tacticlanguage/tactic_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/engine_api.h"
#include "src/engine/unify.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/kernel_api.h"
#include "src/kernel/normalize.h"
#include "src/runtime/core.h"
#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_parser.h"
#include "src/termlanguage/ast_to_expression.h"

/* ============================================================================
 * Pattern bindings: maps pattern variable names -> Expression*
 * ============================================================================ */

#ifndef MAX_PATTERN_BINDINGS
#define MAX_PATTERN_BINDINGS 64
#endif

typedef struct {
    char *names[MAX_PATTERN_BINDINGS];
    Expression *values[MAX_PATTERN_BINDINGS];
    size_t count;
} PatternBindings;

static void bindings_init(PatternBindings *b) { b->count = 0; }

static Expression *bindings_lookup(PatternBindings *b, const char *name) {
    for (size_t i = 0; i < b->count; i++) {
        if (strcmp(b->names[i], name) == 0) {
            return b->values[i];
        }
    }
    return NULL;
}

static bool bindings_add(PatternBindings *b, const char *name, Expression *val) {
    // Check for existing binding - must be consistent
    Expression *existing = bindings_lookup(b, name);
    if (existing) {
        return existing == val;
    }
    if (b->count >= MAX_PATTERN_BINDINGS) {
        return false;
    }
    b->names[b->count] = strdup(name);
    b->values[b->count] = val;
    b->count++;
    return true;
}

static void bindings_free(PatternBindings *b) {
    for (size_t i = 0; i < b->count; i++) {
        free(b->names[i]);
    }
}

/* ============================================================================
 * Pattern matching: match an AST pattern against a kernel Expression
 * ============================================================================ */

static bool _match_pattern(AST *pattern, Expression *expr, PatternBindings *bindings) {
    if (!pattern || !expr) {
        return false;
    }

    switch (pattern->tag) {
        case AST_PATVAR:
            if (strcmp(pattern->value.patvar.name, "_") == 0) {
                return true;
            }
            return bindings_add(bindings, pattern->value.patvar.name, expr);

        case AST_VAR:
            // Match a concrete variable by name
            if (expr->tag != VAR_EXPRESSION) {
                return false;
            }
            return strcmp(pattern->value.var.name, get_var_name(expr)) == 0;

        case AST_TYPE:
            return expr->tag == TYPE_EXPRESSION;

        case AST_PROP:
            return expr->tag == PROP_EXPRESSION;

        case AST_APP:
            if (expr->tag != APP_EXPRESSION) {
                return false;
            }
            return (_match_pattern(pattern->value.app.func, get_app_func(expr), bindings) &&
                    _match_pattern(pattern->value.app.arg, get_app_arg(expr), bindings)) != 0;

        case AST_FORALL:
            if (expr->tag != FORALL_EXPRESSION) {
                return false;
            }
            // Match binder type
            if (pattern->value.forall.binder.type) {
                Expression *bv = get_forall_bound_variable(expr);
                if (!_match_pattern(pattern->value.forall.binder.type, get_expression_type(bv),
                                    bindings)) {
                    return false;
                }
            }
            return _match_pattern(pattern->value.forall.body, get_forall_body(expr), bindings);

        case AST_LAMBDA:
            if (expr->tag != LAMBDA_EXPRESSION) {
                return false;
            }
            if (pattern->value.lambda.binder.type) {
                Expression *bv = get_lambda_bound_variable(expr);
                if (!_match_pattern(pattern->value.lambda.binder.type, get_expression_type(bv),
                                    bindings)) {
                    return false;
                }
            }
            return _match_pattern(pattern->value.lambda.body, get_lambda_body(expr), bindings);

        default:
            return false;
    }
}

/* ============================================================================
 * Flat tactic environment stack
 *
 * Replaces the substitution model (deep-copying body ASTs on every TAC_LET /
 * TAC_CALL).  Push is O(1); lookup is a short linear scan (only a handful of
 * live bindings at any point).  The entries form a linked list via their
 * `next` pointers so the top-of-stack pointer can be passed directly to
 * ast_to_expression_env.
 * ============================================================================ */

#ifndef MAX_ENV_DEPTH
#define MAX_ENV_DEPTH 16384
#endif

static TacticEnvEntry g_env_stack[MAX_ENV_DEPTH];
static int g_env_top = 0;

static TacticEnvEntry *env_top_ptr(void) {
    return g_env_top > 0 ? &g_env_stack[g_env_top - 1] : NULL;
}

/* Lookup from an explicit env-chain head (lexical lookup). */
static TacticEnvEntry *env_lookup_entry_from(TacticEnvEntry *from, const char *name) {
    for (TacticEnvEntry *e = from; e; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return e;
        }
    }
    return NULL;
}

/* Push a name→value binding.  The stack takes ownership of val. */
static void env_push(const char *name, TacticValue *val) {
    if (g_env_top >= MAX_ENV_DEPTH) {
        fprintf(stderr, "[tactic env] stack overflow (MAX_ENV_DEPTH=%d)\n", MAX_ENV_DEPTH);
        abort();
    }
    g_env_stack[g_env_top].name = name;
    g_env_stack[g_env_top].val = val;
    g_env_stack[g_env_top].next = g_env_top > 0 ? &g_env_stack[g_env_top - 1] : NULL;
    g_env_top++;
}

/* Scan from top; return the value (not a dup) or NULL. */
static TacticValue *env_lookup(const char *name) {
    TacticEnvEntry *e = env_lookup_entry_from(env_top_ptr(), name);
    return e ? e->val : NULL;
}

/* Pop back to saved_top, freeing owned TacticValues. */
static void env_pop_to(int saved_top) {
    for (int i = saved_top; i < g_env_top; i++) {
        free_tactic_value(g_env_stack[i].val);
        g_env_stack[i].val = NULL;
    }
    g_env_top = saved_top;
}

/* ============================================================================
 * Primitive tactic dispatch
 *
 * Translates a Tactic AST node into an engine tactic call. This is the
 * bridge from the parsed tactic to the kernel/engine layer.
 * ============================================================================ */

static TacticResult *_interpret_primitive(MEngineRuntime *_, Expression *goal, Tactic *tac) {
    Context *ctx = kernel_expr_context(goal);

    switch (tac->tag) {
        case TACTIC_REWRITE:
        case TACTIC_REWRITE_BACKWARD: {
            Expression *lemma = ast_to_expression_env(tac->as.rewrite.lemma, ctx, env_top_ptr());
            if (!lemma) {
                return init_tactic_result(false, NULL, "Could not resolve rewrite lemma");
            }
            // equiv_proof parsed but not currently used by engine_tactic_rewrite
            return engine_tactic_rewrite(goal, lemma);
        }

        case TACTIC_EREWRITE:
        case TACTIC_EREWRITE_BACKWARD: {
            Expression *lemma = ast_to_expression_env(tac->as.rewrite.lemma, ctx, env_top_ptr());
            if (!lemma) {
                return init_tactic_result(false, NULL, "Could not resolve rewrite lemma");
            }
            return engine_tactic_erewrite(goal, lemma);
        }

        case TACTIC_CBV:
            return engine_tactic_cbv(goal, tac->as.cbv.rules, tac->as.cbv.rules_count);

        case TACTIC_ADMITTED:
            // Admitted is handled specially at the top level, not here
            return init_tactic_result(false, NULL, "Admitted cannot be used in tactic expressions");

        default:
            break;
    }

    return init_tactic_result(false, NULL, "Unknown tactic");
}

/* ============================================================================
 * Tactic interpreter
 * ============================================================================ */

/* Resolve an AST node to a TacticValue.
 * Checks the env stack first (for tactic-let-bound names), then falls back to
 * ast_to_expression_env which can itself see the env for sub-expression lookups.
 * The caller owns the returned TacticValue. */
static TacticValue *resolve_tactic_value_in_env(AST *ast, Context *ctx, TacticEnvEntry *env) {
    if (ast->tag == AST_EXPR_REF) {
        return tactic_value_dup(ast->value.expr_ref.tval);
    }
    if (ast->tag == AST_VAR) {
        TacticEnvEntry *entry = env_lookup_entry_from(env, ast->value.var.name);
        if (entry) {
            TacticValue *found = entry->val;
            if (found->kind == TVAL_AST) {
                /* Resolve in the lexical parent env to avoid self-recursion. */
                return resolve_tactic_value_in_env(found->ast, ctx, entry->next);
            }
            return tactic_value_dup(found);
        }
    }
    Expression *e = ast_to_expression_env(ast, ctx, env);
    if (!e) {
        return NULL;
    }
    return tactic_value_expr(e);
}

static TacticValue *resolve_tactic_value(AST *ast, Context *ctx) {
    return resolve_tactic_value_in_env(ast, ctx, env_top_ptr());
}

#ifndef TAC_CALL_MAX_DEPTH
#define TAC_CALL_MAX_DEPTH 1000
#endif
static int _tac_call_depth = 0;

TacticResult *tactic_interpret(MEngineRuntime *rt, Expression *goal, TacticExpr *expr) {
    switch (expr->tag) {
        case TAC_PRIMITIVE:
            return _interpret_primitive(rt, goal, expr->as.primitive.tactic);

        case TAC_SEQ: {
            // Run left tactic on goal
            TacticResult *left_result = tactic_interpret(rt, goal, expr->as.seq.left);
            if (!tactic_result_get_success(left_result)) {
                return left_result;
            }

            // Run right tactic on each subgoal from left
            DoublyLinkedList *left_goals = tactic_result_get_goals(left_result);
            DoublyLinkedList *all_goals = dll_create();
            TacticValue *last_value = NULL;

            if (left_goals) {
                DLLNode *node = left_goals->head;
                while (node) {
                    Expression *subgoal = (Expression *)node->data;
                    TacticResult *right_result = tactic_interpret(rt, subgoal, expr->as.seq.right);

                    if (!tactic_result_get_success(right_result)) {
                        // Propagate failure
                        dll_destroy(all_goals);
                        free_tactic_value(last_value);
                        free_tactic_result(left_result);
                        return right_result;
                    }

                    DoublyLinkedList *right_goals = tactic_result_get_goals(right_result);
                    // If subgoal was filled (not returned in right_goals), free it.
                    // NULL new_goals means the tactic produced a pure value without touching
                    // the goal (e.g. TAC_PAIR, TAC_FST, TAC_SND). An empty DLL means the
                    // subgoal was actually filled with no remaining subgoals.
                    bool subgoal_filled =
                        (right_goals != NULL && !dll_search(right_goals, subgoal)) != 0;
                    if (right_goals) {
                        all_goals = dll_merge(all_goals, right_goals);
                        right_result->new_goals = NULL;  // consumed by merge
                    }
                    if (right_result->term_value) {
                        free_tactic_value(last_value);
                        last_value = right_result->term_value;
                        right_result->term_value = NULL;  // ownership transferred
                    }
                    free_tactic_result(right_result);
                    if (subgoal_filled) {
                        kernel_free_filled_hole(subgoal);
                    }

                    node = node->next;
                }
            }

            free_tactic_result(left_result);
            TacticResult *seq_result = init_tactic_result(true, all_goals, NULL);
            seq_result->term_value = last_value;
            return seq_result;
        }

        case TAC_ORELSE: {
            // Try left; if it fails, try right (no backtracking)
            TacticResult *left_result = tactic_interpret(rt, goal, expr->as.orelse.left);
            if (tactic_result_get_success(left_result)) {
                return left_result;
            }
            free_tactic_result(left_result);
            return tactic_interpret(rt, goal, expr->as.orelse.right);
        }

        case TAC_TRY: {
            // Try body; if it fails, succeed with goal unchanged
            TacticResult *result = tactic_interpret(rt, goal, expr->as.try_expr.body);
            if (tactic_result_get_success(result)) {
                return result;
            }
            free_tactic_result(result);
            // Succeed with original goal as the sole remaining subgoal
            DoublyLinkedList *goals = dll_create();
            dll_insert_at_tail(goals, dll_new_node(goal));
            return init_tactic_result(true, goals, NULL);
        }

        case TAC_REPEAT: {
            // Repeat body until it fails, collecting final subgoals.
            // Start with the current goal as the set of goals to process.
            DoublyLinkedList *current_goals = dll_create();
            dll_insert_at_tail(current_goals, dll_new_node(goal));

            bool made_progress = true;
            int _repeat_iter = 0;
            while (made_progress) {
                _repeat_iter++;
                if (_repeat_iter > 1000000) {
                    fprintf(stderr, "[repeat] EXCEEDED 1M iterations, aborting\n");
                    break;
                }
                made_progress = false;
                DoublyLinkedList *next_goals = dll_create();

                DLLNode *node = current_goals->head;
                while (node) {
                    Expression *g = (Expression *)node->data;
                    TacticResult *result = tactic_interpret(rt, g, expr->as.repeat.body);

                    if (tactic_result_get_success(result)) {
                        made_progress = true;
                        DoublyLinkedList *new_goals = tactic_result_get_goals(result);
                        // NULL new_goals means pure value computation, g not filled.
                        // An empty DLL means g was filled with no remaining subgoals.
                        bool g_filled = (new_goals != NULL && !dll_search(new_goals, g)) != 0;
                        if (new_goals) {
                            next_goals = dll_merge(next_goals, new_goals);
                            result->new_goals = NULL;  // consumed by merge
                        }
                        // Only free g if it was created internally (not the goal passed in
                        // from outside). The caller owns `goal` and is responsible for freeing it.
                        if (g_filled && g != goal) {
                            kernel_free_filled_hole(g);
                        }
                    } else {
                        // This goal couldn't be processed, keep it
                        dll_insert_at_tail(next_goals, dll_new_node(g));
                    }
                    free_tactic_result(result);

                    node = node->next;
                }

                dll_destroy(current_goals);
                current_goals = next_goals;
            }

            return init_tactic_result(true, current_goals, NULL);
        }

        case TAC_FIRST: {
            // Try each alternative in order, return first success
            for (size_t i = 0; i < expr->as.first.count; i++) {
                TacticResult *result = tactic_interpret(rt, goal, expr->as.first.alternatives[i]);
                if (tactic_result_get_success(result)) {
                    return result;
                }
                free_tactic_result(result);
            }
            return init_tactic_result(false, NULL, "All alternatives in 'first' failed");
        }

        case TAC_IDTAC: {
            // Identity tactic: succeed, goal unchanged
            DoublyLinkedList *goals = dll_create();
            dll_insert_at_tail(goals, dll_new_node(goal));
            return init_tactic_result(true, goals, NULL);
        }

        case TAC_FAIL:
            return init_tactic_result(false, NULL, "fail");

        case TAC_CALL: {
            TacticDef *def =
                tactic_env_lookup(rt->tactic_env, expr->as.call.name, expr->as.call.arg_count);
            if (!def) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Unknown tactic '%s'", expr->as.call.name);
                return init_tactic_result(false, NULL, msg);
            }

            if (expr->as.call.arg_count != def->param_count) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Tactic '%s' expects %zu argument(s), got %zu",
                         expr->as.call.name, def->param_count, expr->as.call.arg_count);
                return init_tactic_result(false, NULL, msg);
            }

            if (_tac_call_depth >= TAC_CALL_MAX_DEPTH) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Tactic recursion limit reached calling '%s'",
                         expr->as.call.name);
                return init_tactic_result(false, NULL, msg);
            }

            // Push each argument as a lazy AST thunk (TVAL_AST).  This avoids
            // eager evaluation, which would fail for literal-name args like
            // `intro P` where P is not yet in the context.  The stored AST is
            // resolved at its use-site via ast_to_expression_env or env_lookup.
            int saved = g_env_top;
            for (size_t i = 0; i < def->param_count; i++) {
                env_push(def->params[i], tactic_value_ast(expr->as.call.args[i]));
            }

            _tac_call_depth++;
            TacticResult *result = def->fn ? def->fn(rt, goal, def->compiled_env)
                                           : tactic_interpret(rt, goal, def->body);
            _tac_call_depth--;
            env_pop_to(saved);
            return result;
        }

        case TAC_MATCH_GOAL: {
            Expression *goal_type = normalize_whnf(get_expression_type(goal));
            Context *ctx = kernel_expr_context(goal);

            for (size_t i = 0; i < expr->as.match_goal.branch_count; i++) {
                GoalBranch *branch = &expr->as.match_goal.branches[i];
                PatternBindings bindings;
                bindings_init(&bindings);

                // Match conclusion pattern against goal type
                if (!_match_pattern(branch->conclusion, goal_type, &bindings)) {
                    bindings_free(&bindings);
                    continue;
                }

                // Match hypothesis patterns against context entries
                bool hyps_matched = true;
                // Per-hyp: record the matched context variable Expression* directly
                Expression **hyp_exprs = NULL;
                size_t hyp_param_count = 0;

                for (size_t j = 0; j < branch->hyp_count; j++) {
                    HypPattern *hp = &branch->hyps[j];
                    bool found = false;
                    Context *c = ctx;

                    while (!context_is_empty(c)) {
                        Expression *hyp_type = normalize_whnf(get_expression_type(c));
                        PatternBindings trial = bindings;
                        if (_match_pattern(hp->type, hyp_type, &trial)) {
                            bindings = trial;
                            hyp_exprs =
                                realloc(hyp_exprs, sizeof(Expression *) * (hyp_param_count + 1));
                            hyp_exprs[hyp_param_count] = c;
                            hyp_param_count++;
                            found = true;
                            break;
                        }
                        c = get_expression_context(c);
                    }

                    if (!found) {
                        hyps_matched = false;
                        break;
                    }
                }

                if (!hyps_matched) {
                    free(hyp_exprs);
                    bindings_free(&bindings);
                    continue;
                }

                // Push all bindings onto the env stack: hypothesis names first,
                // then pattern variable bindings.
                int saved = g_env_top;
                for (size_t k = 0; k < hyp_param_count; k++) {
                    env_push(branch->hyps[k].name, tactic_value_expr(hyp_exprs[k]));
                }
                for (size_t k = 0; k < bindings.count; k++) {
                    env_push(bindings.names[k], tactic_value_expr(bindings.values[k]));
                }
                free(hyp_exprs);

                TacticResult *match_result = tactic_interpret(rt, goal, branch->body);
                env_pop_to(saved);
                /* bindings_free must come AFTER env_pop_to: the env entries
                 * hold borrowed pointers to bindings.names[k] strings. */
                bindings_free(&bindings);
                return match_result;
            }

            return init_tactic_result(false, NULL, "No branch matched in match Goal");
        }

        case TAC_LET: {
            // Evaluate the RHS to get its term value
            TacticResult *rhs_result = tactic_interpret(rt, goal, expr->as.let_expr.rhs);
            if (!tactic_result_get_success(rhs_result)) {
                return rhs_result;
            }
            TacticValue *value = tactic_result_get_value(rhs_result);
            if (!value) {
                free_tactic_result(rhs_result);
                return init_tactic_result(false, NULL,
                                          "let binding RHS did not produce a term value");
            }

            // Save RHS goals before freeing the result struct
            DoublyLinkedList *rhs_goals = tactic_result_get_goals(rhs_result);
            rhs_result->new_goals = NULL;   // ownership transferred
            rhs_result->term_value = NULL;  // ownership transferred to env slot
            free_tactic_result(rhs_result);

            // Push binding onto the env stack and interpret the body.
            // env_pop_to will free value when we unwind.
            int saved = g_env_top;
            env_push(expr->as.let_expr.name, value);
            TacticResult *body_result = tactic_interpret(rt, goal, expr->as.let_expr.body);
            env_pop_to(saved);

            // Merge RHS goals into body result
            if (rhs_goals && tactic_result_get_success(body_result)) {
                DoublyLinkedList *body_goals = tactic_result_get_goals(body_result);
                if (body_goals) {
                    body_result->new_goals = dll_merge(rhs_goals, body_goals);
                } else {
                    body_result->new_goals = rhs_goals;
                }
            } else if (rhs_goals) {
                dll_destroy(rhs_goals);
            }
            return body_result;
        }

        case TAC_GOAL_TYPE: {
            Expression *goal_type = get_expression_type(goal);
            return init_tactic_result_value(tactic_value_expr(goal_type));
        }

        case TAC_TYPE_OF: {
            Context *ctx = kernel_expr_context(goal);
            Expression *term = ast_to_expression_env(expr->as.type_of.term, ctx, env_top_ptr());
            if (!term) {
                return init_tactic_result(false, NULL, "type_of: could not resolve term");
            }
            Expression *type = kernel_expr_type(term);
            return init_tactic_result_value(tactic_value_expr(type));
        }

        case TAC_MATCH_TERM: {
            Context *ctx = kernel_expr_context(goal);
            Expression *scrutinee =
                ast_to_expression_env(expr->as.match_term.scrutinee, ctx, env_top_ptr());
            if (!scrutinee) {
                return init_tactic_result(false, NULL,
                                          "match <term>: could not evaluate scrutinee");
            }

            for (size_t i = 0; i < expr->as.match_term.branch_count; i++) {
                TermBranch *branch = &expr->as.match_term.branches[i];
                PatternBindings bindings;
                bindings_init(&bindings);

                if (!_match_pattern(branch->pattern, scrutinee, &bindings)) {
                    bindings_free(&bindings);
                    continue;
                }

                // Push pattern variable bindings onto the env stack.
                int saved = g_env_top;
                for (size_t k = 0; k < bindings.count; k++) {
                    env_push(bindings.names[k], tactic_value_expr(bindings.values[k]));
                }
                TacticResult *match_result = tactic_interpret(rt, goal, branch->body);
                env_pop_to(saved);
                /* bindings_free must come AFTER env_pop_to: env entries hold
                 * borrowed pointers to bindings.names[k] strings. */
                bindings_free(&bindings);
                return match_result;
            }

            return init_tactic_result(false, NULL, "No branch matched in match <term>");
        }

        case TAC_MK_HOLE: {
            Context *ctx = kernel_expr_context(goal);
            Expression *type = ast_to_expression_env(expr->as.mk_hole.type, ctx, env_top_ptr());
            if (!type) {
                return init_tactic_result(false, NULL, "mk_hole: could not resolve type");
            }
            type = normalize_whnf(type);
            Expression *hole = kernel_hole_create("hole", type, ctx);
            DoublyLinkedList *goals = dll_create();
            dll_insert_at_tail(goals, dll_new_node(hole));
            TacticResult *r = init_tactic_result(true, goals, NULL);
            r->term_value = tactic_value_expr(hole);
            return r;
        }

        case TAC_FILL: {
            Context *ctx = kernel_expr_context(goal);
            Expression *hole = ast_to_expression_env(expr->as.fill.hole, ctx, env_top_ptr());
            Expression *term = ast_to_expression_env(expr->as.fill.term, ctx, env_top_ptr());
            if (!hole || !term) {
                return init_tactic_result(false, NULL, "fill: could not resolve arguments");
            }
            if (!kernel_hole_fill(hole, term)) {
                return init_tactic_result(false, NULL, "fill: hole fill failed");
            }
            return init_tactic_result(true, dll_create(), NULL);
        }

        case TAC_SUBST: {
            Context *ctx = kernel_expr_context(goal);
            Expression *new_term =
                ast_to_expression_env(expr->as.subst.new_term, ctx, env_top_ptr());
            Expression *body = ast_to_expression_env(expr->as.subst.body, ctx, env_top_ptr());
            Expression *old_var = ast_to_expression_env(expr->as.subst.old_var, ctx, env_top_ptr());
            if (!new_term || !body || !old_var) {
                return init_tactic_result(false, NULL, "subst: could not resolve arguments");
            }
            Expression *result = kernel_subst(ctx, body, old_var, new_term);
            if (!result) {
                return init_tactic_result(false, NULL, "subst: substitution failed");
            }
            return init_tactic_result_value(tactic_value_expr(result));
        }

        case TAC_EUNIFY: {
            Context *ctx = kernel_expr_context(goal);
            Expression *lemma_expr =
                ast_to_expression_env(expr->as.eunify.lemma, ctx, env_top_ptr());
            if (!lemma_expr) {
                return init_tactic_result(false, NULL, "eunify: could not resolve lemma");
            }
            UnificationResult *unif = engine_eunify(lemma_expr, goal);
            if (!unif) {
                return init_tactic_result(false, NULL, "eunify: unification failed");
            }
            Expression *inst = engine_unify_get_lemma(unif);
            DoublyLinkedList *new_goals = (DoublyLinkedList *)engine_unify_get_bindings(unif);
            TacticResult *r = init_tactic_result(true, new_goals ? new_goals : dll_create(), NULL);
            r->term_value = tactic_value_expr(inst);
            unif->new_goals = NULL;  // ownership transferred to tactic result
            engine_unify_free(unif);
            return r;
        }

        case TAC_CURRENT_GOAL: {
            TacticResult *r = init_tactic_result(true, dll_create(), NULL);
            r->term_value = tactic_value_expr(goal);
            return r;
        }

        case TAC_INTRO_STEP: {
            Expression *goal_ty = kernel_expr_type(goal);
            Expression *x = kernel_forall_var(goal_ty);
            if (!x) {
                return init_tactic_result(false, NULL,
                                          "intro_step: goal is not a forall expression");
            }

            Expression *A = kernel_expr_type(x);
            Expression *B = kernel_forall_body(goal_ty);

            // Determine the name for the introduced variable
            char *intro_name = kernel_var_name(x);  // default: forall's binder name
            if (expr->as.intro_step.name) {
                AST *name_ast = expr->as.intro_step.name;
                if (name_ast->tag == AST_EXPR_REF) {
                    Expression *name_expr = tactic_value_as_expr(name_ast->value.expr_ref.tval);
                    if (name_expr && name_expr->tag == VAR_EXPRESSION) {
                        intro_name = kernel_var_name(name_expr);
                    }
                } else if (name_ast->tag == AST_VAR) {
                    TacticValue *tv = env_lookup(name_ast->value.var.name);
                    if (tv && tv->kind == TVAL_AST && tv->ast->tag == AST_VAR) {
                        /* Call arg was a literal name, e.g. `intro P` */
                        intro_name = tv->ast->value.var.name;
                    } else if (tv && tv->kind == TVAL_EXPRESSION &&
                               tv->expr->tag == VAR_EXPRESSION) {
                        intro_name = kernel_var_name(tv->expr);
                    } else if (!tv) {
                        /* No binding: use the literal identifier as the name */
                        intro_name = name_ast->value.var.name;
                    }
                }
            }

            Expression *x_prime = kernel_var_create(intro_name, A, kernel_expr_context(goal));
            Expression *B_prime = kernel_subst(x_prime, B, x, x_prime);
            Expression *new_goal = kernel_hole_create((char *)"Goal", B_prime, x_prime);

            Expression *proof = kernel_lambda_create(x_prime, new_goal);
            if (!kernel_hole_fill(goal, proof)) {
                return init_tactic_result(false, NULL, "intro_step: failed to fill the hole");
            }

            DoublyLinkedList *goals = dll_create();
            dll_insert_at_tail(goals, dll_new_node(new_goal));
            return init_tactic_result(true, goals, NULL);
        }

        case TAC_PAIR: {
            Context *ctx = kernel_expr_context(goal);
            TacticValue *fst_val = resolve_tactic_value(expr->as.pair.fst, ctx);
            TacticValue *snd_val = resolve_tactic_value(expr->as.pair.snd, ctx);
            if (!fst_val || !snd_val) {
                free_tactic_value(fst_val);
                free_tactic_value(snd_val);
                return init_tactic_result(false, NULL, "pair: could not resolve arguments");
            }
            return init_tactic_result_value(tactic_value_pair(fst_val, snd_val));
        }

        case TAC_FST: {
            Context *ctx = kernel_expr_context(goal);
            TacticValue *pair_val = resolve_tactic_value(expr->as.fst.term, ctx);
            if (!pair_val || pair_val->kind != TVAL_PAIR) {
                free_tactic_value(pair_val);
                return init_tactic_result(false, NULL, "fst: expected a pair");
            }
            TacticValue *component = tactic_value_dup(pair_val->pair.fst);
            free_tactic_value(pair_val);
            return init_tactic_result_value(component);
        }

        case TAC_SND: {
            Context *ctx = kernel_expr_context(goal);
            TacticValue *pair_val = resolve_tactic_value(expr->as.snd.term, ctx);
            if (!pair_val || pair_val->kind != TVAL_PAIR) {
                free_tactic_value(pair_val);
                return init_tactic_result(false, NULL, "snd: expected a pair");
            }
            TacticValue *component = tactic_value_dup(pair_val->pair.snd);
            free_tactic_value(pair_val);
            return init_tactic_result_value(component);
        }

        case TAC_APP_FUNC: {
            Context *ctx = kernel_expr_context(goal);
            Expression *app_val = ast_to_expression_env(expr->as.app_func.term, ctx, env_top_ptr());
            if (!app_val || app_val->tag != APP_EXPRESSION) {
                return init_tactic_result(false, NULL, "app_func: expected an application");
            }
            return init_tactic_result_value(tactic_value_expr(get_app_func(app_val)));
        }

        case TAC_APP_ARG: {
            Context *ctx = kernel_expr_context(goal);
            Expression *app_val = ast_to_expression_env(expr->as.app_arg.term, ctx, env_top_ptr());
            if (!app_val || app_val->tag != APP_EXPRESSION) {
                return init_tactic_result(false, NULL, "app_arg: expected an application");
            }
            return init_tactic_result_value(tactic_value_expr(get_app_arg(app_val)));
        }

        case TAC_EXPR_EQ: {
            Context *ctx = kernel_expr_context(goal);
            Expression *left = ast_to_expression_env(expr->as.expr_eq.left, ctx, env_top_ptr());
            Expression *right = ast_to_expression_env(expr->as.expr_eq.right, ctx, env_top_ptr());
            if (!left || !right) {
                return init_tactic_result(false, NULL, "expr_eq: could not resolve arguments");
            }
            if (left == right) {
                DoublyLinkedList *goals_list = dll_create();
                dll_insert_at_tail(goals_list, dll_new_node(goal));
                return init_tactic_result(true, goals_list, NULL);
            }
            return init_tactic_result(false, NULL, "expr_eq: expressions are not pointer-equal");
        }

        case TAC_REWRITE_UNIFY: {
            Context *ctx = kernel_expr_context(goal);
            Expression *lemma =
                ast_to_expression_env(expr->as.rewrite_unify.lemma, ctx, env_top_ptr());
            Expression *target =
                ast_to_expression_env(expr->as.rewrite_unify.target, ctx, env_top_ptr());
            if (!lemma || !target) {
                return init_tactic_result(false, NULL,
                                          "rewrite_unify: could not resolve arguments");
            }
            UnificationResult *unif = bad_unify_for_eq(ctx, lemma, target);
            if (!unif) {
                return init_tactic_result(false, NULL, "rewrite_unify: unification failed");
            }
            if (unif->new_goals->head != NULL) {
                free_unification_result(unif);
                return init_tactic_result(false, NULL, "rewrite_unify: unresolved bindings");
            }
            Expression *inst = unif->lemma_instantiation;
            Expression *proof_type = get_expression_type(inst);
            if (!congruence(_get_lhs_eq(proof_type), target)) {
                free_unification_result(unif);
                return init_tactic_result(false, NULL, "rewrite_unify: LHS does not match target");
            }
            free_unification_result(unif);
            return init_tactic_result_value(tactic_value_expr(inst));
        }

        case TAC_CONSTR: {
            Context *ctx = kernel_expr_context(goal);
            Expression *term = ast_to_expression_env(expr->as.constr.term, ctx, env_top_ptr());
            if (!term) {
                return init_tactic_result(false, NULL, "constr: could not resolve term");
            }
            return init_tactic_result_value(tactic_value_expr(term));
        }
        case TAC_DISPATCH:
        case TAC_SHELVE:
            break;
    }

    return init_tactic_result(false, NULL, "Unknown tactic expression");
}
