#ifndef TACTIC_API_H
#define TACTIC_API_H

#include <stdbool.h>

#include "src/common/doubly_linked_list.h"

typedef struct Expression Expression;
typedef struct TacticResult TacticResult;

struct TacticResult {
    bool success;                 // true if the tactic was successful, false otherwise
    DoublyLinkedList *new_goals;  // If success, the new goals, otherwise NULL
    char *error_message;          // If success, NULL, otherwise the error message
    Expression *term_value;       // Optional: term value returned by value-producing primitives
};

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message);
TacticResult *init_tactic_result_value(Expression *value);
void free_tactic_result(TacticResult *result);

/* Accessor functions */
bool tactic_result_get_success(TacticResult *result);
char *tactic_result_get_error_message(TacticResult *result);
DoublyLinkedList *tactic_result_get_goals(TacticResult *result);
Expression *tactic_result_get_value(TacticResult *result);

#endif  // TACTIC_API_H