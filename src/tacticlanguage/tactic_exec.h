#ifndef TACTIC_EXEC_H
#define TACTIC_EXEC_H

#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"

/**
 * Execute a tactic expression in the given runtime.
 *
 * This function takes a parsed TacticExpr*, interprets it against the current
 * goal/proof_state, and may print/use side effects via the engine/kernel.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 * @param tac Pointer to the TacticExpr to execute. Must not be NULL.
 * @return 0 on success, non-zero on error.
 */
int mengine_execute_tactic(MEngineRuntime *rt, TacticExpr *tac);

#endif  // TACTIC_EXEC_H