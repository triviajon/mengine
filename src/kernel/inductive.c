#include "src/kernel/inductive.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/common/color.h"
#include "src/common/dyn_array_map.h"

// Global registry mapping Expression* -> InductiveDefinition*
static Map *inductive_registry = NULL;

void inductive_registry_init(void) {
    if (inductive_registry == NULL) {
        inductive_registry = map_new();
    }
}

bool register_inductive(Expression *inductive_var, Expression **constructors, int constructor_count,
                        Expression *eliminator) {
    if (inductive_var == NULL || constructors == NULL || constructor_count < 1) {
        fprintf(stderr, ERROR "Invalid arguments to register_inductive.\n" CRESET);
        return false;
    }

    if (inductive_registry == NULL) {
        inductive_registry_init();
    }

    if (map_get(inductive_registry, inductive_var) != NULL) {
        fprintf(stderr, ERROR "Inductive type already registered.\n" CRESET);
        return false;
    }

    InductiveDefinition *def = malloc(sizeof(InductiveDefinition));
    if (!def) {
        fprintf(stderr, ERROR "Failed to allocate InductiveDefinition.\n" CRESET);
        return false;
    }

    Expression **ctor_copy = malloc(constructor_count * sizeof(Expression *));
    if (!ctor_copy) {
        fprintf(stderr, ERROR "Failed to allocate constructor array copy.\n" CRESET);
        free(def);
        return false;
    }

    for (int i = 0; i < constructor_count; i++) {
        ctor_copy[i] = constructors[i];
    }

    def->inductive_var = inductive_var;
    def->constructors = ctor_copy;
    def->constructor_count = constructor_count;
    def->eliminator = eliminator;

    map_set(inductive_registry, inductive_var, def);

    return true;
}

bool is_inductive(Expression *expr) {
    if (expr == NULL || inductive_registry == NULL) {
        return false;
    }

    return map_get(inductive_registry, expr) != NULL;
}

Expression **get_constructors(Expression *inductive_var, int *out_count) {
    if (out_count != NULL) {
        *out_count = 0;
    }

    InductiveDefinition *def = get_inductive_definition(inductive_var);
    if (def == NULL) {
        return NULL;
    }

    if (out_count != NULL) {
        *out_count = def->constructor_count;
    }

    return def->constructors;
}

bool is_constructor(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return false;
    }

    Expression *type = get_expression_type(expr);
    Expression *result_type = get_innermost_body(type);
    Expression *inductive_var = get_head(result_type);

    if (!is_inductive(inductive_var)) {
        return false;
    }

    return is_constructor_of(expr, inductive_var);
}
Expression *get_eliminator(Expression *inductive_var) {
    InductiveDefinition *def = get_inductive_definition(inductive_var);
    if (def == NULL) {
        return NULL;
    }

    return def->eliminator;
}

InductiveDefinition *get_inductive_definition(Expression *inductive_var) {
    if (inductive_var == NULL || inductive_registry == NULL) {
        return NULL;
    }

    return (InductiveDefinition *)map_get(inductive_registry, inductive_var);
}

bool is_constructor_of(Expression *expr, Expression *inductive_var) {
    if (expr == NULL || inductive_var == NULL) {
        return false;
    }

    InductiveDefinition *def = get_inductive_definition(inductive_var);
    if (def == NULL) {
        return false;
    }

    // Check if expr is one of this inductive type's constructors
    for (int i = 0; i < def->constructor_count; i++) {
        if (def->constructors[i] == expr) {
            return true;
        }
    }

    return false;
}
