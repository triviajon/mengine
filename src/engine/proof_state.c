#include "src/engine/proof_state.h"

#include <stddef.h>
#include <stdlib.h>

#include "src/engine/unify.h"
#include "src/kernel/kernel_api.h"

ProofState *proof_state_new(Expression *pending_theorem) {
    ProofState *ps = malloc(sizeof(ProofState));
    if (!ps) {
        return NULL;
    }

    ps->pending_theorem = pending_theorem;
    ps->goals = dll_create();
    ps->shelved = dll_create();

    Expression *initial_goal = kernel_var_body(pending_theorem);
    DLLNode *n = dll_new_node(initial_goal);
    dll_insert_at_tail(ps->goals, n);
    ps->current_node = n;

    return ps;
}

void proof_state_free(ProofState *ps) {
    if (!ps) {
        return;
    }

    dll_destroy(ps->goals);
    dll_destroy(ps->shelved);
    free(ps);
}

Expression *proof_state_current(ProofState *ps) {
    if (!ps) {
        return NULL;
    }
    // Advance past any filled holes (cascade-filled evars).
    while (ps->current_node) {
        Expression *goal = (Expression *)ps->current_node->data;
        if (!kernel_expr_is_hole(goal) || !kernel_hole_is_filled(goal)) {
            return goal;
        }
        ps->current_node = ps->current_node->next;
    }
    return NULL;
}

Expression *proof_state_pending_theorem(ProofState *ps) {
    if (!ps) {
        return NULL;
    }
    return ps->pending_theorem;
}

bool proof_state_next(ProofState *ps) {
    if (!ps->current_node || !ps->current_node->next) {
        return false;
    }
    ps->current_node = ps->current_node->next;
    return true;
}

// Check whether hole appears in the type of some other goal in the same batch.
static bool hole_appears_in_other_goal_type(Expression *hole, DoublyLinkedList *batch,
                                            DLLNode *self) {
    for (DLLNode *other = batch->head; other != NULL; other = other->next) {
        if (other == self) {
            continue;
        }
        Expression *other_type = kernel_expr_type((Expression *)other->data);
        DoublyLinkedList *other_type_holes = list_holes(other_type);
        bool found = dll_search(other_type_holes, hole) != NULL;
        dll_destroy(other_type_holes);
        if (found) {
            return true;
        }
    }
    return false;
}

void proof_state_add_goals(ProofState *ps, DoublyLinkedList *new_goals) {
    if (!ps || !new_goals || !new_goals->head) {
        return;
    }

    // Shelve holes that appear in another goal's type within this batch (evar-like
    // arguments); they are expected to be filled via cascade fill when that other
    // goal is solved, so they are recorded but not added to the goal list.
    DLLNode *node = new_goals->head;
    while (node != NULL) {
        DLLNode *next = node->next;
        Expression *goal = (Expression *)node->data;
        if (kernel_expr_is_hole(goal) && hole_appears_in_other_goal_type(goal, new_goals, node)) {
            dll_remove_node(new_goals, node);
            dll_insert_at_tail(ps->shelved, node);
        }
        node = next;
    }

    if (!new_goals->head) {
        free(new_goals);
        return;
    }

    // Insert new goals right after the current goal position so that
    // subgoals from the just-executed tactic are processed before
    // previously-queued goals (e.g., continuation goals from eapply).
    DLLNode *current = ps->current_node;
    if (current) {
        DLLNode *after = current->next;
        // Splice new_goals list between current and after
        current->next = new_goals->head;
        new_goals->head->prev = current;
        if (after) {
            new_goals->tail->next = after;
            after->prev = new_goals->tail;
        } else {
            ps->goals->tail = new_goals->tail;
        }
        free(new_goals);
    } else {
        // No current goal (e.g., empty list), just append
        ps->goals = dll_merge(ps->goals, new_goals);
    }
}

Expression *proof_state_first_unfilled_shelved(ProofState *ps) {
    if (!ps) {
        return NULL;
    }
    for (DLLNode *n = ps->shelved->head; n != NULL; n = n->next) {
        Expression *hole = (Expression *)n->data;
        if (!kernel_hole_is_filled(hole)) {
            return hole;
        }
    }
    return NULL;
}
