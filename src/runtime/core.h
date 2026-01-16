#ifndef CORE_H
#define CORE_H

#include <stdbool.h>

#include "src/kernel/kernel_api.h"

// Core types and constructors hardcoded in C. All of this won't be necessary
// once we implement a tactic writing language

extern Expression *Reflexive;
extern Expression *Reflexive_Definition;

extern Expression *Symmetric;
extern Expression *Symmetric_Definition;

extern Expression *Transitive;
extern Expression *Transitive_Definition;

extern Expression *Equivalence;
extern Expression *Build_Equivalence;

extern Expression *Equivalence_Reflexive;
extern Expression *Equivalence_Symmetric;
extern Expression *Equivalence_Transitive;

extern Expression *Bad_App_Congruence;

extern Expression *eq;
extern Expression *eq_refl;
extern Expression *eq_sym;
extern Expression *eq_trans;
extern Expression *eq_subst;

Expression *_get_lhs_eq(Expression *eq_expression);

Expression *_get_rhs_eq(Expression *eq_expression);

/**
 * Initialize the core library.
 *
 * @param ctx Pointer to the context to initialize.
 */
void init_core(Context **ctx);

#endif  // CORE_H
