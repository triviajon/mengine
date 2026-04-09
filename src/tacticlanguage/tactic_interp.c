#include "src/tacticlanguage/tactic_interp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/doubly_linked_list.h"
#include "src/engine/engine_api.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/kernel_api.h"
#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_parser.h"
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
        const char *name =
            ast->tag == AST_VAR ? ast->value.var.name : ast->value.patvar.name;
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
            copy->value.expr_ref.expr = ast->value.expr_ref.expr;
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
        case TACTIC_APPLY:
            copy->as.apply.lemma = _ast_subst(tac->as.apply.lemma, params, args, count);
            break;
        case TACTIC_EAPPLY:
            copy->as.eapply.lemma = _ast_subst(tac->as.eapply.lemma, params, args, count);
            break;
        case TACTIC_EXACT:
            copy->as.exact.proof_term = _ast_subst(tac->as.exact.proof_term, params, args, count);
            break;
        case TACTIC_REWRITE:
        case TACTIC_REWRITE_BACKWARD:
        case TACTIC_EREWRITE:
        case TACTIC_EREWRITE_BACKWARD:
            copy->as.rewrite.lemma = _ast_subst(tac->as.rewrite.lemma, params, args, count);
            copy->as.rewrite.equiv_proof =
                _ast_subst(tac->as.rewrite.equiv_proof, params, args, count);
            copy->as.rewrite.backward = tac->as.rewrite.backward;
            break;
        case TACTIC_EXISTS:
            copy->as.exists.witness = _ast_subst(tac->as.exists.witness, params, args, count);
            break;
        case TACTIC_INTRO:
            copy->as.intro.name = tac->as.intro.name ? strdup(tac->as.intro.name) : NULL;
            break;
        case TACTIC_INTROS:
            copy->as.intros.name_count = tac->as.intros.name_count;
            if (tac->as.intros.names) {
                copy->as.intros.names = malloc(sizeof(char *) * tac->as.intros.name_count);
                for (size_t i = 0; i < tac->as.intros.name_count; i++) {
                    copy->as.intros.names[i] = strdup(tac->as.intros.names[i]);
                }
            }
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
            // TACTIC_REFLEXIVITY, TACTIC_ASSUMPTION, TACTIC_SPLIT,
            // TACTIC_LEFT, TACTIC_RIGHT, TACTIC_ADMITTED — no AST fields
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
        case TACTIC_INTRO:
            return engine_tactic_intro(goal, tac->as.intro.name);

        case TACTIC_INTROS:
            return engine_tactic_intros(goal, tac->as.intros.names, tac->as.intros.name_count);

        case TACTIC_APPLY: {
            Expression *lemma = ast_to_expression(tac->as.apply.lemma, ctx);
            if (!lemma) {
                return init_tactic_result(false, NULL, "Could not resolve lemma");
            }
            return engine_tactic_apply(goal, lemma);
        }

        case TACTIC_EAPPLY: {
            Expression *lemma = ast_to_expression(tac->as.eapply.lemma, ctx);
            if (!lemma) {
                return init_tactic_result(false, NULL, "Could not resolve lemma");
            }
            return engine_tactic_eapply(goal, lemma);
        }

        case TACTIC_EXACT: {
            Expression *proof_term = ast_to_expression(tac->as.exact.proof_term, ctx);
            if (!proof_term) {
                return init_tactic_result(false, NULL, "Could not resolve proof term");
            }
            return engine_tactic_exact(goal, proof_term);
        }

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

        case TACTIC_REFLEXIVITY:
            return engine_tactic_reflexivity(goal);

        case TACTIC_ASSUMPTION:
            return engine_tactic_assumption(goal);

        case TACTIC_SPLIT:
            return engine_tactic_split(goal);

        case TACTIC_LEFT:
            return engine_tactic_left(goal);

        case TACTIC_RIGHT:
            return engine_tactic_right(goal);

        case TACTIC_EXISTS: {
            Expression *witness = ast_to_expression(tac->as.exists.witness, ctx);
            if (!witness) {
                return init_tactic_result(false, NULL, "Could not resolve witness");
            }
            return engine_tactic_exists(goal, witness);
        }

        case TACTIC_CBV:
            return engine_tactic_cbv(goal, tac->as.cbv.rules, tac->as.cbv.rules_count);

        case TACTIC_ADMITTED:
            // Admitted is handled specially at the top level, not here
            return init_tactic_result(false, NULL, "Admitted cannot be used in tactic expressions");
    }

    return init_tactic_result(false, NULL, "Unknown tactic");
}

/* ============================================================================
 * Tactic interpreter
 * ============================================================================ */

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
                    free_tactic_result(right_result);

                    node = node->next;
                }
            }

            free_tactic_result(left_result);
            return init_tactic_result(true, all_goals, NULL);
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
            TacticDef *def = tactic_env_lookup(rt->tactic_env, expr->as.call.name);
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

            TacticExpr *body = def->body;
            if (def->param_count > 0) {
                body = _tactic_expr_subst(def->body, def->params, expr->as.call.args,
                                          def->param_count);
            }

            return tactic_interpret(rt, goal, body);
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
                        ref->value.expr_ref.expr = bindings.values[k];
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
    }

    return init_tactic_result(false, NULL, "Unknown tactic expression");
}
