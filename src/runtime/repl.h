#ifndef REPL_H
#define REPL_H

#define REPL_LINE_CAP 1024

#include "src/runtime/runtime.h"

/**
 * Run an interactive read–eval–print loop (REPL) on stdin/stdout.
 *
 * The loop terminates on EOF or on an explicit quit command (if supported).
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 */
void mengine_repl(MEngineRuntime *rt);

#endif  // REPL_H