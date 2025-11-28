#ifndef COMMAND_EXEC_H
#define COMMAND_EXEC_H

#include "src/runtime/runtime.h"

/**
 * Execute a single top-level command in the given runtime.
 *
 * This function takes a parsed Command*, elaborates it into kernel expressions
 * as needed, updates the global context, and may print/use side effects via the
 * engine/kernel.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 * @param cmd Pointer to the Command to execute. Must not be NULL.
 */
void mengine_execute_command(MEngineRuntime *rt, Command *cmd);

#endif  // COMMAND_EXEC_H