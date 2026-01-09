#include "src/engine/tactic_api.h"

#include <stdlib.h>

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message) {
    TacticResult *result = malloc(sizeof(TacticResult));
    if (!result) {
        return NULL;
    }

    result->success = success;
    result->new_goals = new_goals;
    result->error_message = error_message;

    return result;
}

void free_tactic_result(TacticResult *result) {
    if (!result) {
        return;
    }
    free(result);
}
