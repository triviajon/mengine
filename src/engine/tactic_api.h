#ifndef TACTIC_API_H
#define TACTIC_API_H

#include <stdbool.h>

#include "src/common/doubly_linked_list.h"

typedef struct {
    bool success;                 // true if the tactic was successful, false otherwise
    DoublyLinkedList *new_goals;  // If success, the new goals, otherwise NULL
    char *error_message;          // If success, NULL, otherwise the error message
} TacticResult;

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message);
void free_tactic_result(TacticResult *result);

#endif  // TACTIC_API_H