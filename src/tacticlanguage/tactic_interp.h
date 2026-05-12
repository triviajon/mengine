#ifndef TACTIC_INTERP_H
#define TACTIC_INTERP_H

#include "src/engine/engine_api.h"
#include "src/runtime/runtime.h"
#include "src/tacticlanguage/tactic_ast.h"

/**
 * Interpret a tactic expression against the current goal.
 *
 * Recursively interprets tactic combinators (seq, orelse, try, repeat, first)
 * and dispatches primitives to the engine tactic API.
 *
 * @param rt  Pointer to the runtime (for context, AST-to-expression, etc.)
 * @param goal The current goal (must be a hole expression)
 * @param expr The tactic expression to interpret
 * @return TacticResult with success/failure and new subgoals
 */
TacticResult *tactic_interpret(MEngineRuntime *rt, Expression *goal, TacticExpr *expr);

#endif  // TACTIC_INTERP_H
