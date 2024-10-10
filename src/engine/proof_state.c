#include "proof_state.h"

#include <stdlib.h>

ProofState *init_proof_state() {
  ProofState *proof_state = malloc(sizeof(ProofState));
  if (!proof_state) {
    return NULL;
  }
  proof_state->holes = dll_create();
  if (!proof_state->holes) {
    free(proof_state);
    return NULL;
  }
  return proof_state;
}

void free_proof_state(ProofState *proof_state) {
  if (proof_state) {
    dll_foreach(proof_state->holes, free_expression);
    dll_destroy(proof_state->holes);
    free(proof_state);
  }
}

void *add_hole(ProofState *proof_state, Expression *hole) {
  dll_insert_at_tail(proof_state->holes, dll_new_node(hole));
}
