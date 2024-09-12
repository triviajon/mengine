#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expression.h"


// Function to typecheck a Let statement
void typecheck_let(char *id, Expression *expr, Program *next) {
    
}

// Function to typecheck a Theorem
void typecheck_theorem(Theorem *theorem, Program *next);

// Function to typecheck the entire program
void typecheck_prog(Program *prog) {
    Program *current = prog;

    while (current != NULL) {
        switch (current->type) {
            case LET_STEP:
                typecheck_let(current->value.let.id, current->value.let.expr, current->next);
                break;

            case THEOREM_STEP:
                typecheck_theorem(current->value.theorem.theorem, current->next);
                break;

            case EXPR_STEP:
                // For now, there should be no need to work with expressions by themselves
                break;
        }
        current = current->next;
    }
}