#include "src/tacticlanguage/tactic_interp.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/common/color.h"
#include "src/common/doubly_linked_list.h"
#include "src/engine/engine_api.h"
#include "src/kernel/kernel_api.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/tacticlanguage/tactic_parser.h"
#include "src/termlanguage/ast_to_expression.h"

/* ============================================================================
 * Primitive tactic dispatch
 *
 * Translates a Tactic AST node into an engine tactic call. This is the
 * bridge from the parsed tactic to the kernel/engine layer.
 * ============================================================================ */

static TacticResult *_interpret_primitive(MEngineRuntime *rt, Expression *goal, Tactic *tac) {
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
                    TacticResult *right_result =
                        tactic_interpret(rt, subgoal, expr->as.seq.right);

                    if (!tactic_result_get_success(right_result)) {
                        // Propagate failure
                        dll_destroy(all_goals);
                        free_tactic_result(left_result);
                        return right_result;
                    }

                    DoublyLinkedList *right_goals = tactic_result_get_goals(right_result);
                    if (right_goals) {
                        dll_merge(all_goals, right_goals);
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
                            dll_merge(next_goals, new_goals);
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
    }

    return init_tactic_result(false, NULL, "Unknown tactic expression");
}
