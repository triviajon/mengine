#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct {
    bool debug;                // Gate all debug options
    bool debug__print_tokens;  // Print tokens during lexing
    bool debug__print_ast;     // Print AST after parsing
} MEngineOptions;

#endif  // OPTIONS_H