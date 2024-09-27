#include "env.h"

Env* env_init() {
    Env *env = (Env*)malloc(sizeof(Env));
    env->head = NULL;
    return env;
}

void env_free(Env *env) {
    EnvEntry *current = env->head;
    while (current != NULL) {
        EnvEntry *next = current->next;
        free(current->id);
        free(current->value);
        free(current);
        current = next;
    }
    free(env);
}

char* env_lookup(Env *env, const char *id) {
    EnvEntry *current = env->head;
    while (current != NULL) {
        if (strcmp(current->id, id) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

void env_insert(Env *env, const char *id, const char *value) {
    EnvEntry *new_entry = (EnvEntry*)malloc(sizeof(EnvEntry));
    new_entry->id = strdup(id);
    new_entry->value = strdup(value);
    new_entry->next = env->head;
    env->head = new_entry;
}

void env_print(Env *env) {
    EnvEntry *current = env->head;
    while (current != NULL) {
        printf("%s = %s\n", current->id, current->value);
        current = current->next;
    }
}
