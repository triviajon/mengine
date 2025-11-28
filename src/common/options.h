#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct {
    bool debug;                // Gate all debug options
    bool debug__print_tokens;  // Print tokens during lexing
    bool debug__print_ast;     // Print AST after parsing
    bool debug__print_mode;  // Print the current runtime mode and relevant info
                             // after each update to the mode.
} MEngineOptions;

#endif  // OPTIONS_H