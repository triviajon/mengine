#include "src/runtime/proof_state.h"

#include <stddef.h>
#include <stdlib.h>

#include "src/kernel/expression.h"

ProofState *proof_state_new(Expression *pending_theorem) {
    ProofState *ps = malloc(sizeof(ProofState));
    if (!ps) {
        return NULL;
    }

    ps->pending_theorem = pending_theorem;
    ps->goals = dll_create();
    ps->goal_index = 0;

    Expression *initial_goal = get_expression_body(pending_theorem);
    DLLNode *n = dll_new_node(initial_goal);
    dll_insert_at_tail(ps->goals, n);

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
    if (ps->goal_index >=
        /*todo: change spec of dll_len */ (size_t)dll_len(ps->goals)) {
        return NULL;
    }
    return (Expression *)dll_at(ps->goals, ps->goal_index)->data;
}

Expression *proof_state_pending_theorem(ProofState *ps) {
    if (!ps) {
        return NULL;
    }
    return ps->pending_theorem;
}

bool proof_state_next(ProofState *ps) {
    size_t len = dll_len(ps->goals);
    if (ps->goal_index + 1 >= len) {
        return false;
    }
    ps->goal_index += 1;
    return true;
}

void proof_state_add_goals(ProofState *ps, DoublyLinkedList *new_goals) {
    if (!ps || !new_goals) {
        return;
    }

    dll_merge(ps->goals, new_goals);
}
