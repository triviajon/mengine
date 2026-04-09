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
    result->term_value = NULL;

    return result;
}

TacticResult *init_tactic_result_value(Expression *value) {
    TacticResult *result = malloc(sizeof(TacticResult));
    if (!result) {
        return NULL;
    }

    result->success = true;
    result->new_goals = NULL;
    result->error_message = NULL;
    result->term_value = value;

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

Expression *tactic_result_get_value(TacticResult *result) {
    if (!result) {
        return NULL;
    }
    return result->term_value;
}
