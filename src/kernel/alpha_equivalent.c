#include "alpha_equivalent.h"
#include "expression.h"

// An env entry proposes that var1 and var2 are alpha-equivalent.
typedef struct EnvEntry {
  Expression *var1;
  Expression *var2;
  struct EnvEntry *next;
} EnvEntry;

typedef struct {
  EnvEntry *head;
} Env;

Env *env_init() {
  Env *env = (Env *)malloc(sizeof(Env));
  env->head = NULL;
  return env;
}

void env_free(Env *env) {
  EnvEntry *current = env->head;
  while (current != NULL) {
    EnvEntry *next = current->next;
    free(current);
    current = next;
  }
  free(env);
}

Expression *env_lookup(Env *env, Expression *var1) {
  EnvEntry *current = env->head;
  while (current != NULL) {
    if (current->var1 == var1) {
      return current->var2;
    }
    current = current->next;
  }
  return NULL;
}

void env_insert(Env *env, Expression *var1, Expression *var2) {
  EnvEntry *new_entry = (EnvEntry *)malloc(sizeof(EnvEntry));
  new_entry->var1 = var1;
  new_entry->var2 = var2;
  new_entry->next = env->head;
  env->head = new_entry;
}

bool _alpha_equivalent(Env *env, Expression *t1, Expression *t2) {
  // switch (t1->type) {
  //   case (VAR_EXPRESSION): return t2->type == VAR_EXPRESSION && env_lookup(env, t1) == t2; 
  // }

  return true; // TODO: Implement
}

bool alpha_equivalent(Expression *t1, Expression *t2) {
  return t1 == t2;  // TODO: Implement
}