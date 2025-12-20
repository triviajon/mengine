#ifndef PROOF_STATE_H
#define PROOF_STATE_H

#include <stddef.h>

#include "src/kernel/expression.h"

typedef struct {
    DoublyLinkedList
        *goals;         // list of Expression* representing proof obligations
    size_t goal_index;  // current goal index
} ProofState;

/**
 * Create a new proof state with a single initial theorem, and adds a TOP_LEVEL
 * uplink for the goal.
 *
 * @param initial_goal The initial theorem expression (should be a
 * VAR_EXPRESSION representing "theorem_name : type").
 * @param initial_context The initial context of the theorem.
 * @return Pointer to the newly allocated ProofState.
 */
ProofState *proof_state_new_from_theorem(Expression *initial_theorem,
                                         Context *initial_context);

/**
 * Create a new proof state with a single initial goal, and adds a TOP_LEVEL
 * uplink for the goal.
 *
 * @param initial_goal The initial goal expression (should be a
 * HOLE_EXPRESSION).
 * @return Pointer to the newly allocated ProofState.
 */
ProofState *proof_state_new(Expression *initial_goal);

/**
 * Free a proof state and all associated resources.
 *
 * @param ps Pointer to the ProofState to free.
 */
void proof_state_free(ProofState *ps);

/**
 * Return the curent goal.
 *
 * @param ps Pointer to the ProofState to free.
 * @return Pointer to the current goal.
 */
Expression *proof_state_current(ProofState *ps);

/**
 * Advance to next goal, if it exists.
 *
 * @param ps Pointer to the ProofState to free.
 * @return true if there is a new goal selected, otherwise false.
 */
bool proof_state_next(ProofState *ps);

/**
 * Append goals to the end of the goals list.
 *
 * @param ps Pointer to the ProofState to free.
 * @param new_goals Pointer to list Expression* representing new proof
 * obligations
 */
void proof_state_add_goals(ProofState *ps, DoublyLinkedList *new_goals);

#endif  // PROOF_STATE_H
