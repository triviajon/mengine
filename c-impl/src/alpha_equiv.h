#ifndef ENV_H
#define ENV_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "expression.h"

typedef struct EnvEntry {
    char *id;
    char *value;
    struct EnvEntry *next;
} EnvEntry;

// Struct representing the environment (a linked list of EnvEntries)
typedef struct {
    EnvEntry *head;
} Env;

// Initialize a new environment
Env* env_init();

// Free the environment and all its entries
void env_free(Env *env);

// Lookup an identifier in the environment
char* env_lookup(Env *env, const char *id);

// Insert a key-value pair into the environment (replaces if id already exists)
void env_insert(Env *env, const char *id, const char *value);

// Print the environment
void env_print(Env *env);

bool alpha_equivalent(Expression *t1, Expression *t2);

#endif // ENV_H
