#ifndef PROOF_STATE_H
#define PROOF_STATE_H

#include "doubly_linked_list.h"
#include "expression.h"

typedef struct {
  DoublyLinkedList *holes;
} ProofState;

ProofState *init_proof_state();
void free_proof_state(ProofState *proof_state);

int num_holes(ProofState *proof_state);
int get_hole_at(ProofState *proof_state, int hole_index);
void add_hole(ProofState *proof_state, Expression *hole);

#endif  // PROOF_STATE_H
