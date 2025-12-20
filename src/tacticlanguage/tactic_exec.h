#ifndef TACTIC_EXEC_H
#define TACTIC_EXEC_H

#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_parser.h"

/**
 * Execute a single top-level tactic in the given runtime.
 *
 * This function takes a parsed Tactic*, applies it to the current
 * goal/proof_state, and may print/use side effects via the engine/kernel.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 * @param tac Pointer to the Tactic to execute. Must not be NULL.
 */
void mengine_execute_tactic(MEngineRuntime *rt, Tactic *tac);

#endif  // TACTIC_EXEC_H