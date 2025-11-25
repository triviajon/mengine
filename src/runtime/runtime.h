#ifndef RUNTIME_H
#define RUNTIME_H

#include "src/commandlanguage/command_parser.h"
#include "src/kernel/context.h"
#include "src/kernel/utils.h"
#include "src/metalanguage/ast_to_expression.h"

#define REPL_LINE_CAP 1024

typedef struct {
    Context *ctx;
} MEngineRuntime;

/**
 * Allocate and initialize a new MEngine runtime.
 *
 * @return Pointer to a newly allocated MEngineRuntime, or NULL on error.
 */
MEngineRuntime *mengine_runtime_new(void);

/**
 * Free all resources associated with the runtime.
 *
 * @param rt Pointer to the runtime to free. May be NULL.
 */
void mengine_runtime_free(MEngineRuntime *rt);

/**
 * Access the runtime's current context.
 *
 * @param rt Pointer to the runtime.
 * @return Pointer to the Context, or NULL if rt is NULL.
 */
Context *mengine_runtime_context(MEngineRuntime *rt);

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

/**
 * Run an interactive read–eval–print loop (REPL) on stdin/stdout.
 *
 * The loop terminates on EOF or on an explicit quit command (if supported).
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 */
void mengine_repl(MEngineRuntime *rt);

#endif  // RUNTIME_H
