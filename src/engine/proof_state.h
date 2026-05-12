#ifndef PROOF_STATE_H
#define PROOF_STATE_H

#include <stddef.h>

#include "src/common/doubly_linked_list.h"
#include "src/kernel/kernel_api.h"

typedef struct ProofState ProofState;

struct ProofState {
    Expression *pending_theorem;
    DoublyLinkedList *goals;  // list of Expression* representing proof obligations
    DLLNode *current_node;    // pointer to current (or last-visited) node; NULL = exhausted
};

/**
 * Create a new proof state to prove the pending theorem.
 *
 * @param pending_theorem The pending theorem expression (variable) whose body
 * should be a hole.
 * @return Pointer to the newly allocated ProofState.
 */
ProofState *proof_state_new(Expression *pending_theorem);

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
 * Return the pending theorem, whose body may be partially filled.
 *
 * @param ps Pointer to the ProofState to free.
 * @return Pointer to the original goal.
 */
Expression *proof_state_pending_theorem(ProofState *ps);

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
