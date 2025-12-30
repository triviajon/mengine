#ifndef INDUCTIVE_H
#define INDUCTIVE_H

#include <stdbool.h>

#include "src/kernel/expression.h"

// Represents a registered inductive type definition
typedef struct {
    Expression *inductive_var;  // The variable representing the inductive type
    Expression **constructors;  // Array of constructor variables (registry owns copy)
    int constructor_count;      // Number of constructors
    Expression *eliminator;     // The eliminator/recursor (can be NULL)
} InductiveDefinition;

// Initialize the inductive registry (call once at startup)
void inductive_registry_init(void);

// Register a new inductive type with its constructors
// Makes a copy of the constructors array (caller retains ownership)
// Returns true on success, false if already registered
bool register_inductive(Expression *inductive_var, Expression **constructors, int constructor_count,
                        Expression *eliminator);

// Check if an expression is a registered inductive type
bool is_inductive(Expression *expr);

// Check if an expression is a constructor
bool is_constructor(Expression *expr);

// Get the constructors of an inductive type
// Returns NULL if not an inductive type
// Sets out_count to the number of constructors
Expression **get_constructors(Expression *inductive_var, int *out_count);

// Get the eliminator of an inductive type
// Returns NULL if not an inductive type or no eliminator registered
Expression *get_eliminator(Expression *inductive_var);

// Get the full inductive definition
// Returns NULL if not an inductive type
InductiveDefinition *get_inductive_definition(Expression *inductive_var);

// Check if an expression is a constructor of the specified inductive type
// Returns false if inductive_var is not a registered inductive type
bool is_constructor_of(Expression *expr, Expression *inductive_var);

#endif  // INDUCTIVE_H