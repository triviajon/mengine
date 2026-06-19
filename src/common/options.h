#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

#define MPRINT(quiet, stream, fmt, ...)      \
    if (!quiet) {                            \
        fprintf(stream, fmt, ##__VA_ARGS__); \
    }

typedef enum {
    MENGINE_EXECUTION_TYPE_FILE,
    MENGINE_EXECUTION_TYPE_REPL,
} MEngineExecutionType;

typedef struct {
    bool debug;                // Gate all debug options
    bool debug__print_tokens;  // Print tokens during lexing
    bool debug__print_ast;     // Print AST after parsing
    bool debug__print_mode;    // Print the current runtime mode and relevant info
                               // after each update to the mode.
    bool quiet;                // If true, suppresses all printing to stdout
    bool time;                 // If true, print subsystem timing summary on exit
    bool bare;                 // If true, load nothing at startup (no core, no prelude)
    MEngineExecutionType execution_type;
} MEngineOptions;

#endif  // OPTIONS_H