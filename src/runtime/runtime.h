#ifndef RUNTIME_H
#define RUNTIME_H

#include "src/common/options.h"
#include "src/engine/proof_state.h"

typedef enum {
    MENGINE_RUNTIME_COMMAND_MODE,  // Normal mode of the Mengine Runtime, where
                                   // we are expecting commands such as
                                   // definitions, statements (theorem
                                   // statements), and checks.
    MENGINE_RUNTIME_PROOF_MODE,    // Proof mode of the Mengine Runtime, where wem
                                   // are exclusively expecting tactic language
                                   // commands.
} MEngineRuntimeMode;

typedef struct {
    MEngineOptions *options;      // Runtime options
    Context *ctx;                 // current runtime context
    Expression *pending_theorem;  // when in proof mode, this holds a reference to the
                                  // theorem yet to be proven. It is expected that when
                                  // the proof script is complete, and the runtime
                                  // transitions back to command mode, we will add this
                                  // expression to the runtime context and clear this
                                  // field.
    ProofState *proof_state;      // when in proof mode, this holds a reference to
                                  // the current state of the proof being created
                                  // for pending_theorem.
    MEngineRuntimeMode mode;      // current mode of operation, see MEngineRuntimeMode
} MEngineRuntime;

void debug_print_mode(MEngineRuntime *rt);

/**
 * Allocate and initialize a new MEngine runtime.
 * @param options Pointer to MEngine Options.
 * @return Pointer to a newly allocated MEngineRuntime, or NULL on error.
 */
MEngineRuntime *mengine_runtime_new(MEngineOptions *options);

/**
 * Free all resources associated with the runtime.
 *
 * @param rt Pointer to the runtime to free. May be NULL.
 */
void mengine_runtime_free(MEngineRuntime *rt);

/**
 * Execute a string of MEngine commands/statements.
 *
 * @param rt Pointer to an initialized runtime.
 * @param source Null-terminated string containing the program.
 * @return 0 on success, non-zero on error.
 */
int mengine_runtime_exec_string(MEngineRuntime *rt, const char *source);

/**
 * Execute a file of MEngine commands/statements.
 *
 * @param rt Pointer to an initialized runtime.
 * @param filename Path to the file.
 * @return 0 on success, non-zero on error.
 */
int mengine_runtime_exec_file(MEngineRuntime *rt, const char *filename);

/**
 * Access the runtime's current context.
 *
 * @param rt Pointer to the runtime.
 * @return Pointer to the Context, or NULL if rt is NULL.
 */
Context *mengine_runtime_context(MEngineRuntime *rt);

/**
 * Switch runtime to proof mode.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 * @param pending_theorem
 */
void mengine_runtime_proof_mode(MEngineRuntime *rt, Expression *pending_theorem);

/**
 * Switch runtime to command mode.
 *
 * @param rt Pointer to the runtime. Must not be NULL.
 */
void mengine_runtime_command_mode(MEngineRuntime *rt);

#endif  // RUNTIME_H
