#include "src/kernel/inductive.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/common/color.h"
#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/expression.h"

// Global registry mapping Expression* -> InductiveDefinition*
static LinearMap *inductive_registry = NULL;

void inductive_registry_init(void) {
    if (inductive_registry == NULL) {
        inductive_registry = linear_map_new();
    }
}

void inductive_registry_shutdown(void) {
    if (inductive_registry == NULL) {
        return;
    }
    for (int i = 0; i < inductive_registry->size; i++) {
        InductiveDefinition *def = (InductiveDefinition *)inductive_registry->items[i].val;
        if (def) {
            free(def->constructors);
            free(def);
        }
    }
    linear_map_clear_free(inductive_registry);
    inductive_registry = NULL;
}

static bool occurs_rec(Expression *var, Expression *expr, Map *visited) {
    if (!expr || map_get(visited, expr)) {
        return false;
    }
    map_set(visited, expr, expr);
    switch (expr->tag) {
        case VAR_EXPRESSION:
            return expr == var;
        case APP_EXPRESSION:
            return occurs_rec(var, get_app_func(expr), visited) ||
                   occurs_rec(var, get_app_arg(expr), visited);
        case LAMBDA_EXPRESSION:
            return occurs_rec(var, get_expression_type(get_lambda_bound_variable(expr)), visited) ||
                   occurs_rec(var, get_lambda_body(expr), visited);
        case FORALL_EXPRESSION:
            return occurs_rec(var, get_expression_type(get_forall_bound_variable(expr)), visited) ||
                   occurs_rec(var, get_forall_body(expr), visited);
        case MATCH_EXPRESSION:
            if (occurs_rec(var, expr->as.match.scrutinee, visited)) {
                return true;
            }
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                if (occurs_rec(var, expr->as.match.branches[i]->body, visited)) {
                    return true;
                }
            }
            return false;
        case FIX_EXPRESSION:
            for (int i = 0; i < expr->as.fix.arg_count; i++) {
                if (occurs_rec(var, get_expression_type(expr->as.fix.args[i]), visited)) {
                    return true;
                }
            }
            return occurs_rec(var, expr->as.fix.body, visited);
        default:
            return false;
    }
}

// Whether var occurs in expr, including in binder types.
static bool occurs(Expression *var, Expression *expr) {
    Map *visited = map_new();
    bool result = occurs_rec(var, expr, visited);
    map_free(visited);
    return result;
}

// Whether type is `inductive a1 ... am` with inductive occurring in no ai.
static bool is_plain_application_of(Expression *inductive, Expression *type) {
    while (type->tag == APP_EXPRESSION) {
        if (occurs(inductive, get_app_arg(type))) {
            return false;
        }
        type = get_app_func(type);
    }
    return type == inductive;
}

// A constructor argument type is strictly positive if the inductive does not occur in it, or it
// is `forall (y1 : B1) ... (yk : Bk), inductive a1 ... am` with the inductive in no Bj or ai.
static bool is_strictly_positive(Expression *inductive, Expression *arg_type) {
    while (arg_type->tag == FORALL_EXPRESSION) {
        if (occurs(inductive, get_expression_type(get_forall_bound_variable(arg_type)))) {
            return false;
        }
        arg_type = get_forall_body(arg_type);
    }
    return !occurs(inductive, arg_type) || is_plain_application_of(inductive, arg_type);
}

// After its parameters, a constructor type must take strictly positive arguments and conclude
// with the inductive itself.
static bool is_valid_constructor_type(Expression *inductive, int param_count,
                                      Expression *ctor_type) {
    for (int i = 0; i < param_count; i++) {
        if (ctor_type->tag != FORALL_EXPRESSION) {
            return false;
        }
        ctor_type = get_forall_body(ctor_type);
    }
    while (ctor_type->tag == FORALL_EXPRESSION) {
        if (!is_strictly_positive(inductive,
                                  get_expression_type(get_forall_bound_variable(ctor_type)))) {
            return false;
        }
        ctor_type = get_forall_body(ctor_type);
    }
    return is_plain_application_of(inductive, ctor_type);
}

bool register_inductive(Expression *inductive_var, int param_count, Expression **constructors,
                        int constructor_count, Expression *eliminator) {
    if (inductive_var == NULL || constructors == NULL || constructor_count < 1) {
        fprintf(stderr, ERROR "Invalid arguments to register_inductive.\n" CRESET);
        return false;
    }

    if (inductive_registry == NULL) {
        inductive_registry_init();
    }

    if (linear_map_get(inductive_registry, inductive_var) != NULL) {
        fprintf(stderr, ERROR "Inductive type already registered.\n" CRESET);
        return false;
    }

    // Check strict positivity of each constructor.
    for (int i = 0; i < constructor_count; i++) {
        if (!is_valid_constructor_type(inductive_var, param_count,
                                       get_expression_type(constructors[i]))) {
            fprintf(stderr, ERROR "Constructor %s of %s is not strictly positive.\n" CRESET,
                    get_var_name(constructors[i]), get_var_name(inductive_var));
            return false;
        }
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
    def->param_count = param_count;
    def->constructors = ctor_copy;
    def->constructor_count = constructor_count;
    def->eliminator = eliminator;

    linear_map_set(inductive_registry, inductive_var, def);

    return true;
}

bool is_inductive(Expression *expr) {
    if (expr == NULL || inductive_registry == NULL) {
        return false;
    }

    // For direct inductive vars, check registry
    if (linear_map_get(inductive_registry, expr) != NULL) {
        return true;
    }

    // For parametric inductive applications like (sep_list A), extract the head
    Expression *head = expr;
    while (head != NULL && head->tag == APP_EXPRESSION) {
        head = get_app_func(head);
    }

    // Check if the head is an inductive
    if (head != NULL && head != expr) {
        return linear_map_get(inductive_registry, head) != NULL;
    }

    return false;
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

    // Try direct lookup first
    InductiveDefinition *def =
        (InductiveDefinition *)linear_map_get(inductive_registry, inductive_var);
    if (def != NULL) {
        return def;
    }

    // For parametric inductive applications like (sep_list A), extract the head
    Expression *head = inductive_var;
    while (head != NULL && head->tag == APP_EXPRESSION) {
        head = get_app_func(head);
    }

    // Check if the head is an inductive definition
    if (head != NULL && head != inductive_var) {
        return (InductiveDefinition *)linear_map_get(inductive_registry, head);
    }

    return NULL;
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
