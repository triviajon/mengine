#include "src/runtime/proof_state.h"

static void proof_state_register_top_level_uplink(Expression *goal) {
    Uplink *ul = new_uplink_tl();
    add_to_parents(goal, ul);
}

ProofState *proof_state_new_from_theorem(Expression *initial_theorem,
                                         Context *initial_context) {
    Expression *proof = init_hole_expression(
        "Goal", get_expression_type(initial_theorem), initial_context);

    return proof_state_new(proof);
}

ProofState *proof_state_new(Expression *initial_goal) {
    ProofState *ps = malloc(sizeof(ProofState));
    if (!ps) return NULL;

    ps->goals = dll_create();
    ps->goal_index = 0;

    DLLNode *n = dll_new_node(initial_goal);
    dll_insert_at_tail(ps->goals, n);

    proof_state_register_top_level_uplink(initial_goal);

    return ps;
}

void proof_state_free(ProofState *ps) {
    if (!ps) return;

    DLLNode *node = ps->goals->head;
    while (node) {
        Expression *goal = (Expression *)node->data;

        remove_tl_uplink(goal);

        node = node->next;
    }

    dll_destroy(ps->goals);

    free(ps);
}

Expression *proof_state_current(ProofState *ps) {
    if (!ps) return NULL;
    if (ps->goal_index >= dll_len(ps->goals)) {
        return NULL;
    }
    return (Expression *)dll_at(ps->goals, ps->goal_index)->data;
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
    if (!ps || !new_goals) return;

    dll_merge(ps->goals, new_goals);
}
