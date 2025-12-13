#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include "src/kernel/context.h"
#include "src/kernel/expression.h"

// Core types and constructors hardcoded in C (not parsed from .me files).

extern Expression *Equivalence;
extern Expression *Build_Equivalence;
extern Expression *Equiv_App_Cong;

/**
 * Initialize the core library.
 *
 * @param ctx Pointer to the context to initialize.
 */
void init_core(Context **ctx);

#endif  // CORE_H
