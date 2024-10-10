#include "theorem.h"

Theorem *init_theorem(char *name, Expression *theorem, Expression *proof) {
    Theorem *new_theorem = (Theorem *)malloc(sizeof(Theorem));
    if (new_theorem == NULL) {
        return NULL;
    }

    new_theorem->name = (char *)malloc(strlen(name) + 1);
    if (new_theorem->name == NULL) {
        free(new_theorem);
        return NULL;
    }
    strcpy(new_theorem->name, name);

    new_theorem->theorem = theorem;
    new_theorem->proof = proof;

    return new_theorem;
}