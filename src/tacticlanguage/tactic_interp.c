#include "src/tacticlanguage/tactic_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/engine_api.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/kernel_api.h"
#include "src/kernel/normalize.h"
#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_parser.h"
#include "src/engine/unify.h"
#include "src/runtime/core.h"
#include "src/termlanguage/ast_to_expression.h"

/* ============================================================================
 * Pattern bindings: maps pattern variable names -> Expression*
 * ============================================================================ */

#define MAX_PATTERN_BINDINGS 64

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
    // Check for existing binding — must be consistent
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
            return _match_pattern(pattern->value.app.func, get_app_func(expr), bindings) &&
                   _match_pattern(pattern->value.app.arg, get_app_arg(expr), bindings);

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
 * AST deep-copy with variable substitution
 *
 * Used to instantiate tactic definition bodies by replacing parameter
 * identifiers with argument ASTs.
 * ============================================================================ */

static AST *_ast_subst(AST *ast, char **params, AST **args, size_t count);

static AST *_ast_deep_copy(AST *ast) { return _ast_subst(ast, NULL, NULL, 0); }

static Binder *_binder_subst(Binder *b, char **params, AST **args, size_t count) {
    Binder *copy = malloc(sizeof(Binder));
    copy->name = b->name ? strdup(b->name) : NULL;
    copy->type = _ast_subst(b->type, params, args, count);
    return copy;
}

static Pattern *_pattern_copy(Pattern *p) {
    if (!p) {
        return NULL;
    }
    Pattern *copy = malloc(sizeof(Pattern));
    copy->constructor_name = p->constructor_name ? strdup(p->constructor_name) : NULL;
    copy->argument_count = p->argument_count;
    copy->argument_names = NULL;
    if (p->argument_count > 0 && p->argument_names) {
        copy->argument_names = malloc(sizeof(char *) * p->argument_count);
        for (int i = 0; i < p->argument_count; i++) {
            copy->argument_names[i] = p->argument_names[i] ? strdup(p->argument_names[i]) : NULL;
        }
    }
    return copy;
}

static AST *_ast_subst(AST *ast, char **params, AST **args, size_t count) {
    if (!ast) {
        return NULL;
    }

    // Check for variable substitution
    if ((ast->tag == AST_VAR || ast->tag == AST_PATVAR) && params) {
        const char *name = ast->tag == AST_VAR ? ast->value.var.name : ast->value.patvar.name;
        for (size_t i = 0; i < count; i++) {
            if (strcmp(name, params[i]) == 0) {
                return _ast_deep_copy(args[i]);
            }
        }
    }

    AST *copy = malloc(sizeof(AST));
    copy->tag = ast->tag;

    switch (ast->tag) {
        case AST_VAR:
            copy->value.var.name = strdup(ast->value.var.name);
            break;
        case AST_TYPE:
        case AST_PROP:
            break;
        case AST_APP:
            copy->value.app.func = _ast_subst(ast->value.app.func, params, args, count);
            copy->value.app.arg = _ast_subst(ast->value.app.arg, params, args, count);
            break;
        case AST_LAMBDA:
            copy->value.lambda.binder.name =
                ast->value.lambda.binder.name ? strdup(ast->value.lambda.binder.name) : NULL;
            copy->value.lambda.binder.type =
                _ast_subst(ast->value.lambda.binder.type, params, args, count);
            copy->value.lambda.body = _ast_subst(ast->value.lambda.body, params, args, count);
            break;
        case AST_FORALL:
            copy->value.forall.binder.name =
                ast->value.forall.binder.name ? strdup(ast->value.forall.binder.name) : NULL;
            copy->value.forall.binder.type =
                _ast_subst(ast->value.forall.binder.type, params, args, count);
            copy->value.forall.body = _ast_subst(ast->value.forall.body, params, args, count);
            break;
        case AST_LET:
            copy->value.let.name = ast->value.let.name ? strdup(ast->value.let.name) : NULL;
            copy->value.let.type = _ast_subst(ast->value.let.type, params, args, count);
            copy->value.let.value = _ast_subst(ast->value.let.value, params, args, count);
            copy->value.let.body = _ast_subst(ast->value.let.body, params, args, count);
            break;
        case AST_FIX:
            copy->value.fix.name = ast->value.fix.name ? strdup(ast->value.fix.name) : NULL;
            copy->value.fix.decreasing_arg_name = ast->value.fix.decreasing_arg_name
                                                      ? strdup(ast->value.fix.decreasing_arg_name)
                                                      : NULL;
            copy->value.fix.binder_count = ast->value.fix.binder_count;
            copy->value.fix.binders = malloc(sizeof(Binder *) * ast->value.fix.binder_count);
            for (size_t i = 0; i < ast->value.fix.binder_count; i++) {
                copy->value.fix.binders[i] =
                    _binder_subst(ast->value.fix.binders[i], params, args, count);
            }
            copy->value.fix.return_type =
                _ast_subst(ast->value.fix.return_type, params, args, count);
            copy->value.fix.body = _ast_subst(ast->value.fix.body, params, args, count);
            break;
        case AST_MATCH:
            copy->value.match.scrutinee =
                _ast_subst(ast->value.match.scrutinee, params, args, count);
            copy->value.match.branch_count = ast->value.match.branch_count;
            copy->value.match.branches = malloc(sizeof(AST *) * ast->value.match.branch_count);
            for (size_t i = 0; i < ast->value.match.branch_count; i++) {
                copy->value.match.branches[i] =
                    _ast_subst(ast->value.match.branches[i], params, args, count);
            }
            break;
        case AST_MATCHBRANCH:
            copy->value.matchbranch.pattern = _pattern_copy(ast->value.matchbranch.pattern);
            copy->value.matchbranch.body =
                _ast_subst(ast->value.matchbranch.body, params, args, count);
            break;
        case AST_PATVAR:
            copy->value.patvar.name = strdup(ast->value.patvar.name);
            break;
        case AST_EXPR_REF:
            copy->value.expr_ref.tval = ast->value.expr_ref.tval;
            break;
    }

    return copy;
}

/* ============================================================================
 * Tactic / TacticExpr substitution
 * ============================================================================ */

static Tactic *_tactic_subst(Tactic *tac, char **params, AST **args, size_t count) {
    if (!tac || count == 0) {
        return tac;
    }

    Tactic *copy = malloc(sizeof(Tactic));
    *copy = *tac;  // shallow copy

    switch (tac->tag) {
        case TACTIC_REWRITE:
        case TACTIC_REWRITE_BACKWARD:
        case TACTIC_EREWRITE:
        case TACTIC_EREWRITE_BACKWARD:
            copy->as.rewrite.lemma = _ast_subst(tac->as.rewrite.lemma, params, args, count);
            copy->as.rewrite.equiv_proof =
                _ast_subst(tac->as.rewrite.equiv_proof, params, args, count);
            copy->as.rewrite.backward = tac->as.rewrite.backward;
            break;
        case TACTIC_CBV:
            copy->as.cbv.rules_count = tac->as.cbv.rules_count;
            if (tac->as.cbv.rules) {
                copy->as.cbv.rules = malloc(sizeof(char *) * tac->as.cbv.rules_count);
                for (int i = 0; i < tac->as.cbv.rules_count; i++) {
                    copy->as.cbv.rules[i] = strdup(tac->as.cbv.rules[i]);
                }
            }
            break;
        default:
            // TACTIC_ADMITTED — no AST fields
            break;
    }

    return copy;
}

static TacticExpr *_tactic_expr_subst(TacticExpr *expr, char **params, AST **args, size_t count) {
    if (!expr || count == 0) {
        return expr;
    }

    switch (expr->tag) {
        case TAC_PRIMITIVE:
            return tactic_expr_primitive(
                _tactic_subst(expr->as.primitive.tactic, params, args, count));
        case TAC_SEQ:
            return tactic_expr_seq(_tactic_expr_subst(expr->as.seq.left, params, args, count),
                                   _tactic_expr_subst(expr->as.seq.right, params, args, count));
        case TAC_ORELSE:
            return tactic_expr_orelse(
                _tactic_expr_subst(expr->as.orelse.left, params, args, count),
                _tactic_expr_subst(expr->as.orelse.right, params, args, count));
        case TAC_TRY:
            return tactic_expr_try(_tactic_expr_subst(expr->as.try_expr.body, params, args, count));
        case TAC_REPEAT:
            return tactic_expr_repeat(
                _tactic_expr_subst(expr->as.repeat.body, params, args, count));
        case TAC_FIRST: {
            TacticExpr **alts = malloc(sizeof(TacticExpr *) * expr->as.first.count);
            for (size_t i = 0; i < expr->as.first.count; i++) {
                alts[i] = _tactic_expr_subst(expr->as.first.alternatives[i], params, args, count);
            }
            return tactic_expr_first(alts, expr->as.first.count);
        }
        case TAC_CALL: {
            // Substitute in the call's arguments
            AST **new_args = NULL;
            if (expr->as.call.arg_count > 0) {
                new_args = malloc(sizeof(AST *) * expr->as.call.arg_count);
                for (size_t i = 0; i < expr->as.call.arg_count; i++) {
                    new_args[i] = _ast_subst(expr->as.call.args[i], params, args, count);
                }
            }
            return tactic_expr_call(expr->as.call.name, new_args, expr->as.call.arg_count);
        }
        case TAC_IDTAC:
            return tactic_expr_idtac();
        case TAC_FAIL:
            return tactic_expr_fail();
        case TAC_MATCH_GOAL: {
            size_t bc = expr->as.match_goal.branch_count;
            GoalBranch *new_branches = malloc(sizeof(GoalBranch) * bc);
            for (size_t i = 0; i < bc; i++) {
                GoalBranch *old = &expr->as.match_goal.branches[i];
                new_branches[i].hyp_count = old->hyp_count;
                new_branches[i].hyps = NULL;
                if (old->hyp_count > 0) {
                    new_branches[i].hyps = malloc(sizeof(HypPattern) * old->hyp_count);
                    for (size_t j = 0; j < old->hyp_count; j++) {
                        new_branches[i].hyps[j].name = strdup(old->hyps[j].name);
                        new_branches[i].hyps[j].type =
                            _ast_subst(old->hyps[j].type, params, args, count);
                    }
                }
                new_branches[i].conclusion = _ast_subst(old->conclusion, params, args, count);
                new_branches[i].body = _tactic_expr_subst(old->body, params, args, count);
            }
            return tactic_expr_match_goal(new_branches, bc);
        }
        case TAC_LET:
            return tactic_expr_let(expr->as.let_expr.name,
                                   _tactic_expr_subst(expr->as.let_expr.rhs, params, args, count),
                                   _tactic_expr_subst(expr->as.let_expr.body, params, args, count));
        case TAC_GOAL_TYPE:
            return tactic_expr_goal_type();
        case TAC_TYPE_OF:
            return tactic_expr_type_of(_ast_subst(expr->as.type_of.term, params, args, count));
        case TAC_MATCH_TERM: {
            size_t bc = expr->as.match_term.branch_count;
            TermBranch *new_branches = malloc(sizeof(TermBranch) * bc);
            for (size_t i = 0; i < bc; i++) {
                TermBranch *old = &expr->as.match_term.branches[i];
                new_branches[i].pattern = _ast_subst(old->pattern, params, args, count);
                new_branches[i].body = _tactic_expr_subst(old->body, params, args, count);
            }
            return tactic_expr_match_term(
                _ast_subst(expr->as.match_term.scrutinee, params, args, count), new_branches, bc);
        }
        case TAC_MK_HOLE:
            return tactic_expr_mk_hole(_ast_subst(expr->as.mk_hole.type, params, args, count));
        case TAC_FILL:
            return tactic_expr_fill(_ast_subst(expr->as.fill.hole, params, args, count),
                                    _ast_subst(expr->as.fill.term, params, args, count));
        case TAC_SUBST:
            return tactic_expr_subst(_ast_subst(expr->as.subst.new_term, params, args, count),
                                     _ast_subst(expr->as.subst.body, params, args, count),
                                     _ast_subst(expr->as.subst.old_var, params, args, count));
        case TAC_EUNIFY:
            return tactic_expr_eunify(_ast_subst(expr->as.eunify.lemma, params, args, count));
        case TAC_CURRENT_GOAL:
            return tactic_expr_current_goal();
        case TAC_INTRO_STEP:
            return tactic_expr_intro_step(
                expr->as.intro_step.name
                    ? _ast_subst(expr->as.intro_step.name, params, args, count)
                    : NULL);
        case TAC_PAIR:
            return tactic_expr_pair(_ast_subst(expr->as.pair.fst, params, args, count),
                                    _ast_subst(expr->as.pair.snd, params, args, count));
        case TAC_FST:
            return tactic_expr_fst(_ast_subst(expr->as.fst.term, params, args, count));
        case TAC_SND:
            return tactic_expr_snd(_ast_subst(expr->as.snd.term, params, args, count));
        case TAC_APP_FUNC:
            return tactic_expr_app_func(_ast_subst(expr->as.app_func.term, params, args, count));
        case TAC_APP_ARG:
            return tactic_expr_app_arg(_ast_subst(expr->as.app_arg.term, params, args, count));
        case TAC_EXPR_EQ:
            return tactic_expr_expr_eq(_ast_subst(expr->as.expr_eq.left, params, args, count),
                                       _ast_subst(expr->as.expr_eq.right, params, args, count));
        case TAC_REWRITE_UNIFY:
            return tactic_expr_rewrite_unify(
                _ast_subst(expr->as.rewrite_unify.lemma, params, args, count),
                _ast_subst(expr->as.rewrite_unify.target, params, args, count));
        case TAC_CONSTR:
            return tactic_expr_constr(_ast_subst(expr->as.constr.term, params, args, count));
    }

    return expr;
}

/* ============================================================================
 * Primitive tactic dispatch
 *
 * Translates a Tactic AST node into an engine tactic call. This is the
 * bridge from the parsed tactic to the kernel/engine layer.
 * ============================================================================ */

static TacticResult *_interpret_primitive(MEngineRuntime *rt, Expression *goal, Tactic *tac) {
    (void)rt;
    Context *ctx = kernel_expr_context(goal);

    switch (tac->tag) {
        case TACTIC_REWRITE:
        case TACTIC_REWRITE_BACKWARD: {
            Expression *lemma = ast_to_expression(tac->as.rewrite.lemma, ctx);
            if (!lemma) {
                return init_tactic_result(false, NULL, "Could not resolve rewrite lemma");
            }
            // equiv_proof parsed but not currently used by engine_tactic_rewrite
            return engine_tactic_rewrite(goal, lemma);
        }

        case TACTIC_EREWRITE:
        case TACTIC_EREWRITE_BACKWARD: {
            Expression *lemma = ast_to_expression(tac->as.rewrite.lemma, ctx);
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
 * If the node is an AST_EXPR_REF, return the wrapped TacticValue directly.
 * Otherwise, evaluate via ast_to_expression and wrap as TVAL_EXPRESSION. */
static TacticValue *resolve_tactic_value(AST *ast, Context *ctx) {
    if (ast->tag == AST_EXPR_REF) {
        return ast->value.expr_ref.tval;
    }
    Expression *e = ast_to_expression(ast, ctx);
    if (!e) return NULL;
    return tactic_value_expr(e);
}

#define TAC_CALL_MAX_DEPTH 1000
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
                        free_tactic_result(left_result);
                        return right_result;
                    }

                    DoublyLinkedList *right_goals = tactic_result_get_goals(right_result);
                    if (right_goals) {
                        all_goals = dll_merge(all_goals, right_goals);
                    }
                    if (right_result->term_value) {
                        last_value = right_result->term_value;
                    }
                    free_tactic_result(right_result);

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
            while (made_progress) {
                made_progress = false;
                DoublyLinkedList *next_goals = dll_create();

                DLLNode *node = current_goals->head;
                while (node) {
                    Expression *g = (Expression *)node->data;
                    TacticResult *result = tactic_interpret(rt, g, expr->as.repeat.body);

                    if (tactic_result_get_success(result)) {
                        made_progress = true;
                        DoublyLinkedList *new_goals = tactic_result_get_goals(result);
                        if (new_goals) {
                            next_goals = dll_merge(next_goals, new_goals);
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
            TacticDef *def = tactic_env_lookup(rt->tactic_env, expr->as.call.name,
                                                 expr->as.call.arg_count);
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

            TacticExpr *body = def->body;
            if (def->param_count > 0) {
                body = _tactic_expr_subst(def->body, def->params, expr->as.call.args,
                                          def->param_count);
            }

            _tac_call_depth++;
            TacticResult *result = tactic_interpret(rt, goal, body);
            _tac_call_depth--;
            return result;
        }

        case TAC_MATCH_GOAL: {
            Expression *goal_type = get_expression_type(goal);
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
                // For each hyp pattern, we need to find a context entry that matches.
                // Track which hyp pattern names map to which context variable names.
                char **hyp_param_names = NULL;
                AST **hyp_param_values = NULL;
                size_t hyp_param_count = 0;

                for (size_t j = 0; j < branch->hyp_count; j++) {
                    HypPattern *hp = &branch->hyps[j];
                    bool found = false;
                    Context *c = ctx;

                    while (!context_is_empty(c)) {
                        Expression *hyp_type = get_expression_type(c);
                        PatternBindings trial = bindings;
                        // Try matching this hypothesis type
                        if (_match_pattern(hp->type, hyp_type, &trial)) {
                            // Match succeeded — record the binding
                            bindings = trial;
                            hyp_param_names =
                                realloc(hyp_param_names, sizeof(char *) * (hyp_param_count + 1));
                            hyp_param_values =
                                realloc(hyp_param_values, sizeof(AST *) * (hyp_param_count + 1));
                            hyp_param_names[hyp_param_count] = hp->name;
                            AST *var_ast = malloc(sizeof(AST));
                            var_ast->tag = AST_VAR;
                            var_ast->value.var.name = strdup(get_var_name(c));
                            hyp_param_values[hyp_param_count] = var_ast;
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
                    free(hyp_param_names);
                    free(hyp_param_values);
                    bindings_free(&bindings);
                    continue;
                }

                // Substitute hypothesis name bindings AND pattern variable
                // bindings into the body.  Hypothesis names map to AST_VAR
                // nodes (already collected above); pattern variables map to
                // AST_EXPR_REF nodes wrapping the matched Expression*.
                size_t total_count = hyp_param_count + bindings.count;
                char **all_names = NULL;
                AST **all_values = NULL;

                if (total_count > 0) {
                    all_names = malloc(sizeof(char *) * total_count);
                    all_values = malloc(sizeof(AST *) * total_count);

                    // Copy hypothesis name bindings
                    for (size_t k = 0; k < hyp_param_count; k++) {
                        all_names[k] = hyp_param_names[k];
                        all_values[k] = hyp_param_values[k];
                    }

                    // Convert pattern variable bindings to AST_EXPR_REF
                    for (size_t k = 0; k < bindings.count; k++) {
                        all_names[hyp_param_count + k] = bindings.names[k];
                        AST *ref = malloc(sizeof(AST));
                        ref->tag = AST_EXPR_REF;
                        ref->value.expr_ref.tval = tactic_value_expr(bindings.values[k]);
                        all_values[hyp_param_count + k] = ref;
                    }
                }

                TacticExpr *body = branch->body;
                if (total_count > 0) {
                    body = _tactic_expr_subst(body, all_names, all_values, total_count);
                }

                free(all_names);
                free(all_values);
                free(hyp_param_names);
                free(hyp_param_values);
                bindings_free(&bindings);

                return tactic_interpret(rt, goal, body);
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

            // Substitute the bound name in the body with AST_EXPR_REF wrapping
            // the value.
            AST *ref = malloc(sizeof(AST));
            ref->tag = AST_EXPR_REF;
            ref->value.expr_ref.tval = value;

            char *params[1] = {expr->as.let_expr.name};
            AST *args[1] = {ref};
            TacticExpr *body = _tactic_expr_subst(expr->as.let_expr.body, params, args, 1);

            free(rhs_result);  // free struct only, not the goals list we saved
            TacticResult *body_result = tactic_interpret(rt, goal, body);

            // Merge RHS goals into body result
            if (rhs_goals && tactic_result_get_success(body_result)) {
                DoublyLinkedList *body_goals = tactic_result_get_goals(body_result);
                if (body_goals) {
                    body_result->new_goals = dll_merge(rhs_goals, body_goals);
                } else {
                    body_result->new_goals = rhs_goals;
                }
            }
            return body_result;
        }

        case TAC_GOAL_TYPE: {
            Expression *goal_type = get_expression_type(goal);
            return init_tactic_result_value(tactic_value_expr(goal_type));
        }

        case TAC_TYPE_OF: {
            Context *ctx = kernel_expr_context(goal);
            Expression *term = ast_to_expression(expr->as.type_of.term, ctx);
            if (!term) {
                return init_tactic_result(false, NULL, "type_of: could not resolve term");
            }
            Expression *type = kernel_expr_type(term);
            return init_tactic_result_value(tactic_value_expr(type));
        }

        case TAC_MATCH_TERM: {
            Context *ctx = kernel_expr_context(goal);
            Expression *scrutinee = ast_to_expression(expr->as.match_term.scrutinee, ctx);
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

                // Convert pattern variable bindings to AST_EXPR_REF substitutions
                char **names = NULL;
                AST **refs = NULL;
                if (bindings.count > 0) {
                    names = malloc(sizeof(char *) * bindings.count);
                    refs = malloc(sizeof(AST *) * bindings.count);
                    for (size_t k = 0; k < bindings.count; k++) {
                        names[k] = bindings.names[k];
                        AST *ref = malloc(sizeof(AST));
                        ref->tag = AST_EXPR_REF;
                        ref->value.expr_ref.tval = tactic_value_expr(bindings.values[k]);
                        refs[k] = ref;
                    }
                }

                TacticExpr *body = branch->body;
                if (bindings.count > 0) {
                    body = _tactic_expr_subst(body, names, refs, bindings.count);
                }

                free(names);
                free(refs);
                bindings_free(&bindings);
                return tactic_interpret(rt, goal, body);
            }

            return init_tactic_result(false, NULL, "No branch matched in match <term>");
        }

        case TAC_MK_HOLE: {
            Context *ctx = kernel_expr_context(goal);
            Expression *type = ast_to_expression(expr->as.mk_hole.type, ctx);
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
            Expression *hole = ast_to_expression(expr->as.fill.hole, ctx);
            Expression *term = ast_to_expression(expr->as.fill.term, ctx);
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
            Expression *new_term = ast_to_expression(expr->as.subst.new_term, ctx);
            Expression *body = ast_to_expression(expr->as.subst.body, ctx);
            Expression *old_var = ast_to_expression(expr->as.subst.old_var, ctx);
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
            Expression *lemma_expr = ast_to_expression(expr->as.eunify.lemma, ctx);
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
                if (name_ast->tag == AST_VAR) {
                    intro_name = name_ast->value.var.name;
                } else if (name_ast->tag == AST_EXPR_REF) {
                    Expression *name_expr = tactic_value_as_expr(name_ast->value.expr_ref.tval);
                    if (name_expr->tag == VAR_EXPRESSION) {
                        intro_name = kernel_var_name(name_expr);
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
                return init_tactic_result(false, NULL, "pair: could not resolve arguments");
            }
            return init_tactic_result_value(tactic_value_pair(fst_val, snd_val));
        }

        case TAC_FST: {
            Context *ctx = kernel_expr_context(goal);
            TacticValue *pair_val = resolve_tactic_value(expr->as.fst.term, ctx);
            if (!pair_val || pair_val->kind != TVAL_PAIR) {
                return init_tactic_result(false, NULL, "fst: expected a pair");
            }
            return init_tactic_result_value(pair_val->pair.fst);
        }

        case TAC_SND: {
            Context *ctx = kernel_expr_context(goal);
            TacticValue *pair_val = resolve_tactic_value(expr->as.snd.term, ctx);
            if (!pair_val || pair_val->kind != TVAL_PAIR) {
                return init_tactic_result(false, NULL, "snd: expected a pair");
            }
            return init_tactic_result_value(pair_val->pair.snd);
        }

        case TAC_APP_FUNC: {
            Context *ctx = kernel_expr_context(goal);
            Expression *app_val = ast_to_expression(expr->as.app_func.term, ctx);
            if (!app_val || app_val->tag != APP_EXPRESSION) {
                return init_tactic_result(false, NULL, "app_func: expected an application");
            }
            return init_tactic_result_value(tactic_value_expr(get_app_func(app_val)));
        }

        case TAC_APP_ARG: {
            Context *ctx = kernel_expr_context(goal);
            Expression *app_val = ast_to_expression(expr->as.app_arg.term, ctx);
            if (!app_val || app_val->tag != APP_EXPRESSION) {
                return init_tactic_result(false, NULL, "app_arg: expected an application");
            }
            return init_tactic_result_value(tactic_value_expr(get_app_arg(app_val)));
        }

        case TAC_EXPR_EQ: {
            Context *ctx = kernel_expr_context(goal);
            Expression *left = ast_to_expression(expr->as.expr_eq.left, ctx);
            Expression *right = ast_to_expression(expr->as.expr_eq.right, ctx);
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
            Expression *lemma = ast_to_expression(expr->as.rewrite_unify.lemma, ctx);
            Expression *target = ast_to_expression(expr->as.rewrite_unify.target, ctx);
            if (!lemma || !target) {
                return init_tactic_result(false, NULL,
                                          "rewrite_unify: could not resolve arguments");
            }
            UnificationResult *unif = bad_unify_for_eq(ctx, lemma, target);
            if (!unif) {
                return init_tactic_result(false, NULL,
                                          "rewrite_unify: unification failed");
            }
            if (dll_len(unif->new_goals) > 0) {
                free_unification_result(unif);
                return init_tactic_result(false, NULL,
                                          "rewrite_unify: unresolved bindings");
            }
            Expression *inst = unif->lemma_instantiation;
            Expression *proof_type = get_expression_type(inst);
            if (!congruence(_get_lhs_eq(proof_type), target)) {
                free_unification_result(unif);
                return init_tactic_result(false, NULL,
                                          "rewrite_unify: LHS does not match target");
            }
            free_unification_result(unif);
            return init_tactic_result_value(tactic_value_expr(inst));
        }

        case TAC_CONSTR: {
            Context *ctx = kernel_expr_context(goal);
            Expression *term = ast_to_expression(expr->as.constr.term, ctx);
            if (!term) {
                return init_tactic_result(false, NULL, "constr: could not resolve term");
            }
            return init_tactic_result_value(tactic_value_expr(term));
        }
    }

    return init_tactic_result(false, NULL, "Unknown tactic expression");
}
