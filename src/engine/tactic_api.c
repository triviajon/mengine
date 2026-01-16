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

bool tactic_result_get_success(TacticResult *result) {
    if (!result) {
        return false;
    }
    return result->success;
}

char *tactic_result_get_error_message(TacticResult *result) {
    if (!result) {
        return NULL;
    }
    return result->error_message;
}

DoublyLinkedList *tactic_result_get_goals(TacticResult *result) {
    if (!result) {
        return NULL;
    }
    return result->new_goals;
}
