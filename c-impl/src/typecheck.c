#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "expression.h"

Expression *typecheck_expression_under_context(Expression *gamma, Expression *expr); 


// Returns true if delta is a valid context in the valid context gamma
bool typecheck_context(Expression *gamma, Expression *delta) {
    // Typecheck delta under gamma, as in https://www.cs.cmu.edu/%7Efp/papers/mfps89.pdf
    switch (gamma->type) {
        case (TYPE_EXPRESSION):
            switch (delta->type) {
                case (TYPE_EXPRESSION):
                    return true;
                case (FORALL_EXPRESSION):
                    Expression *new_gamma = set_in_context(gamma, delta->value.forall.var, delta->value.forall.type);
                    return typecheck_context(new_gamma, delta->value.forall.arg);
                default:
                    return false;
            }
        case (FORALL_EXPRESSION):
            switch (delta->type) {
                case (TYPE_EXPRESSION):
                    Expression *delta_prime = gamma->value.forall.type;
                    Expression *gamma_prime = gamma->value.forall.arg;
                    if (is_valid_context(delta_prime)) {
                        return typecheck_context(gamma_prime, delta_prime);
                    } else {
                        return typecheck_expression_under_context(gamma_prime, delta_prime)->type == TYPE_EXPRESSION;
                    }
            }

    }
}

void typecheck_non_context(Expression *context, Expression *expr) {

}

bool is_valid_context(Expression *expr) {
    switch (expr->type) {
        case TYPE_EXPRESSION: 
            return true;
        case FORALL_EXPRESSION:
            return is_valid_context(expr->value.forall.arg);
        default:
            return false;
    }
}

void typecheck_expression(Expression *expr) {
    if (is_valid_context(expr)) {
        typecheck_valid_context(expr);
    } else {
        Expression *context = init_type_expression();
        typecheck_non_context(context, expr);
        free_type_expression(context);
    }
}

// Function to typecheck a Let statement
void typecheck_let(char *id, Expression *expr, Program *next) {
    
}

// Function to typecheck a Theorem
void typecheck_theorem(Theorem *theorem, Program *next) {
    // To typecheck a theorem, we must make sure that it's
    // theorem term (type) is well-typed, and that it's proof is a 
    // term of the theorem type

    typecheck_expression(theorem->theorem);

    Expression *new_theorem_term = beta_reduction(theorem->theorem);
}

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