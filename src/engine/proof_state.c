#include "src/engine/proof_state.h"

#include <stddef.h>
#include <stdlib.h>

#include "src/kernel/kernel_api.h"

ProofState *proof_state_new(Expression *pending_theorem) {
    ProofState *ps = malloc(sizeof(ProofState));
    if (!ps) {
        return NULL;
    }

    ps->pending_theorem = pending_theorem;
    ps->goals = dll_create();

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
    free(ps);
}

Expression *proof_state_current(ProofState *ps) {
    if (!ps) {
        return NULL;
    }
    // Advance past any satisfied holes (cascade-filled evars).
    while (ps->current_node) {
        Expression *goal = (Expression *)ps->current_node->data;
        if (!kernel_expr_is_hole(goal) || !kernel_hole_is_satisfied(goal)) {
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

void proof_state_add_goals(ProofState *ps, DoublyLinkedList *new_goals) {
    if (!ps || !new_goals || !new_goals->head) {
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
