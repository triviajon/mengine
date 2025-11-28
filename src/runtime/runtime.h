#ifndef RUNTIME_H
#define RUNTIME_H

#include "src/commandlanguage/command_parser.h"
#include "src/common/options.h"
#include "src/kernel/context.h"
#include "src/kernel/utils.h"
#include "src/metalanguage/ast_to_expression.h"
#include "src/runtime/definition_table.h"
#include "src/tacticlanguage/tactic_parser.h"

#define REPL_LINE_CAP 1024

typedef enum {
    MENGINE_RUNTIME_COMMAND_MODE,  // Normal mode of the Mengine Runtime, where
                                   // we are expecting commands such as
                                   // definitions, statements (theorem
                                   // statements), and checks.
    MENGINE_RUNTIME_PROOF_MODE,  // Proof mode of the Mengine Runtime, where we
                                 // are exclusively expecting tactic language
                                 // commands.
} MEngineRuntimeMode;

typedef struct {
    MEngineOptions *options;  // Runtime options
    Context *ctx;             // current runtime context
    DefinitionTable
        *def_table;  // table of definitions (bindings from strings to terms)
    Expression
        *pending_theorem;  // when in proof mode, this holds a reference to the
                           // theorem yet to be proven. It is expected that when
                           // the proof script is complete, and the runtime
                           // transitions back to command mode, we will add this
                           // expression to the runtime context and clear this
                           // field.
    MEngineRuntimeMode
        mode;  // current mode of operation, see MEngineRuntimeMode
} MEngineRuntime;

void debug_print_mode_update(MEngineRuntime *rt);

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
 * Switch runtime to proof mode.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 * @param theorem Pointer to theorem to be proven (variable). Must not be NULL.
 */
void mengine_runtime_proof_mode(MEngineRuntime *rt, Expression *theorem);

/**
 * Switch runtime to command mode.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 */
void mengine_runtime_command_mode(MEngineRuntime *rt);

/**
 * Run an interactive read–eval–print loop (REPL) on stdin/stdout.
 *
 * The loop terminates on EOF or on an explicit quit command (if supported).
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 */
void mengine_repl(MEngineRuntime *rt);

#endif  // RUNTIME_H
