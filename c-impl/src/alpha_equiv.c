#include "alpha_equiv.h"

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

bool _alpha_equivalent(Env *env, Expression *t1, Expression *t2) {
  if (t1->type != t2->type) {
    return false;
  }

  switch (t1->type) {
  case TYPE_EXPRESSION:
    return true;

  case VAR_EXPRESSION: {
    if (equal(t1, t2)) {
      return true;
    }
    return strcmp(env_lookup(env, t1->value.var.name),
                  env_lookup(env, t2->value.var.name)) == 0;
  }

  case LAMBDA_EXPRESSION:
  case FORALL_EXPRESSION: {
    char *name1 = t1->value.lambda.var->value.var.name;
    char *name2 = t2->value.lambda.var->value.var.name;

    env_insert(env, name1, name2);

    return _alpha_equivalent(env, t1->value.lambda.var_type,
                            t2->value.lambda.var_type) &&
           _alpha_equivalent(env, t1->value.lambda.body, t2->value.lambda.body);
  }

  case APP_EXPRESSION:
    return _alpha_equivalent(env, t1->value.app.func, t2->value.app.func) &&
           _alpha_equivalent(env, t1->value.app.arg, t2->value.app.arg);

  default:
    return false;
  }
}

bool alpha_equivalent(Expression *t1, Expression *t2) {
    Env *env = env_init();
    bool result = _alpha_equivalent(env, t1, t2);
    env_free(env);
    return result;
}