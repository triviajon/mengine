#include "src/kernel/expression.h"

#include <stdio.h>

#include "src/common/color.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/dyn_array_map.h"
#include "src/kernel/new_subst.h"

void add_to_parents(Expression *expression, void *ptr, Relation r) {
    Uplink *uplink = new_uplink(ptr, r);
    switch (expression->type) {
        case (VAR_EXPRESSION):
            dll_insert_at_head(expression->value.var.uplinks,
                               dll_new_node(uplink));
            break;
        case (LAMBDA_EXPRESSION):
            dll_insert_at_head(expression->value.lambda.uplinks,
                               dll_new_node(uplink));
            break;
        case (APP_EXPRESSION):
            dll_insert_at_head(expression->value.app.uplinks,
                               dll_new_node(uplink));
            break;
        case (FORALL_EXPRESSION):
            dll_insert_at_head(expression->value.forall.uplinks,
                               dll_new_node(uplink));
            break;
        case (TYPE_EXPRESSION):
        case (PROP_EXPRESSION):
            break;
        case (HOLE_EXPRESSION):
            dll_insert_at_head(expression->value.hole.uplinks,
                               dll_new_node(uplink));
            break;
        default:
            fprintf(stderr, ERROR
                    "Unknown expression type in add_to_parents.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

void remove_tl_uplink(Expression *expression) {
    (void)expression;
    // todo: implement me
}

Uplink *new_uplink(void *ptr, Relation r) {
    Uplink *new_uplink = malloc(sizeof(Uplink));
    if (!new_uplink) {
        return NULL;
    }

    new_uplink->ptr = ptr;
    new_uplink->relation = r;
    return new_uplink;
}

// Helper to construct a lambda type from a bound variable and body.
// Assumes all inputs are valid.
Expression *_construct_lambda_type(Expression *bound_variable, Expression *body,
                                   Context *gamma) {
    return init_forall_expression_wc(bound_variable, get_expression_type(body),
                                     gamma);
}

// Helper to construct a app type from a function and argument.
// Assumes all inputs are valid.
Expression *_construct_app_type(Expression *func, Expression *arg) {
    Expression *func_type = get_expression_type(func);  // Forall x: A, B
    Expression *weak_func_type = weak_head_normalize(func_type);
    if (weak_func_type->type != FORALL_EXPRESSION) {
        fprintf(stderr, ERROR "Trying to apply a non-function.\n" CRESET);
        return NULL;
    }
    Expression *variable = get_forall_bound_variable(weak_func_type);  // x
    Expression *expected_arg_type = get_expression_type(variable);     // A
    Expression *actual_arg_type = get_expression_type(arg);            // A?
    Expression *return_type = get_forall_body(weak_func_type);         // B

    if (subtypes(actual_arg_type, expected_arg_type)) {
        return new_subst(return_type, variable, arg);  // B[x -> arg]
    }

    fprintf(stderr, ERROR "Application does not type check.\n" CRESET);
    return NULL;
}

Expression *init_prop_expression() {
    if (PROP == NULL) {
        PROP = (Expression *)malloc(sizeof(Expression));
        PROP->type = PROP_EXPRESSION;
        PROP->value.type.uplinks = dll_create();
    }
    return PROP;
}

Expression *init_type_expression() {
    if (TYPE == NULL) {
        TYPE = (Expression *)malloc(sizeof(Expression));
        TYPE->type = TYPE_EXPRESSION;
        TYPE->value.type.uplinks = dll_create();
    }
    return TYPE;
}

Expression *init_hole_expression(char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->type != PROP_EXPRESSION &&
        type_type->type != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = HOLE_EXPRESSION;
    expr->value.hole.name = name;
    expr->value.hole.context = gamma;
    expr->value.hole.type = type;
    add_to_parents(type, expr, HOLE_TYPE);
    expr->value.hole.uplinks = dll_create();
    expr->value.hole.maybe_hole_free = false;
    return expr;
}

Expression *init_var_expression_wc(const char *name, Expression *type,
                                   Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->type != PROP_EXPRESSION &&
        type_type->type != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.type = type;
    expr->value.var.uplinks = dll_create();
    expr->value.var.context = gamma;
    expr->value.var.maybe_hole_free = true;
    return expr;
}

Expression *init_var_expression_wc_with_definition(const char *name,
                                                   Expression *definition,
                                                   Context *gamma) {
    if (!valid_in_context(definition, gamma)) {
        fprintf(stderr, ERROR "Definition is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type = get_expression_type(definition);
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->type != PROP_EXPRESSION &&
        type_type->type != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.definition = definition;
    add_to_parents(definition, expr, VAR_BODY);
    expr->value.var.type = type;
    expr->value.var.uplinks = dll_create();
    expr->value.var.context = gamma;
    expr->value.var.maybe_hole_free = true;
    return expr;
}

Expression *init_lambda_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *gamma) {
    Context *extended_with_bound_variable =
        context_insert(gamma, bound_variable);

    if (!extended_with_bound_variable) {
        fprintf(stderr,
                ERROR "Failed to extend context with bound variable.\n" CRESET);
        return NULL;
    }

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = LAMBDA_EXPRESSION;
    expr->value.lambda.context = gamma;
    expr->value.lambda.bound_variable = bound_variable;
    expr->value.lambda.type =
        _construct_lambda_type(bound_variable, body, gamma);
    expr->value.lambda.body = body;
    add_to_parents(body, expr, LAMBDA_BODY);
    expr->value.lambda.uplinks = dll_create();
    expr->value.lambda.maybe_hole_free = get_maybe_hole_free(body);
    return expr;
}

Expression *init_app_expression_wc(Expression *func, Expression *arg,
                                   Context *context) {
    if (!valid_in_context(func, context)) {
        fprintf(stderr, ERROR "Function is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *func_type = get_expression_type(func);
    if (func_type->type != FORALL_EXPRESSION) {
        fprintf(stderr, ERROR "Function is not a Forall expression.\n" CRESET);
        return NULL;
    }

    if (!valid_in_context(arg, context)) {
        fprintf(stderr, ERROR "Argument is not valid in context.\n" CRESET);
        return NULL;
    }

    // We perform the verification that type of the argument is a subtype of the
    // function's bound variable type in the _construct_app_type helper.
    Expression *type = _construct_app_type(func, arg);
    if (!type) {
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = APP_EXPRESSION;
    Context *combined_ctx = context;
    expr->value.app.context = combined_ctx;
    expr->value.app.func = func;
    add_to_parents(func, expr, APP_FUNC);
    expr->value.app.arg = arg;
    add_to_parents(arg, expr, APP_ARG);
    expr->value.app.type = type;
    expr->value.app.cache = NULL;
    expr->value.app.uplinks = dll_create();
    expr->value.app.maybe_hole_free =
        get_maybe_hole_free(func) && get_maybe_hole_free(arg);
    return expr;
}

Expression *init_forall_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *gamma) {
    Context *extended_with_bound_variable =
        context_insert(gamma, bound_variable);

    if (!extended_with_bound_variable) {
        fprintf(stderr,
                ERROR "Failed to extend context with bound variable.\n" CRESET);
        return NULL;
    }

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *body_type = get_expression_type(body);
    if (body_type->type != PROP_EXPRESSION &&
        body_type->type != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Body type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *bound_variable_type = get_expression_type(bound_variable);
    if (bound_variable_type->type == TYPE_EXPRESSION &&
        !valid_in_context(bound_variable_type, gamma)) {
        fprintf(stderr,
                ERROR "Bound variable type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = FORALL_EXPRESSION;
    expr->value.forall.context = gamma;
    expr->value.forall.bound_variable = bound_variable;
    expr->value.forall.type = body_type;
    expr->value.forall.body = body;
    add_to_parents(body, expr, FORALL_BODY);
    expr->value.forall.uplinks = dll_create();
    expr->value.forall.maybe_hole_free = get_maybe_hole_free(body);
    return expr;
}

Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs,
                                     Context *gamma) {
    // lhs -> rhs <-> Forall _: lhs, rhs
    Expression *unnamed_variable = init_var_expression_wc("_", lhs, gamma);
    return init_forall_expression_wc(unnamed_variable, rhs, gamma);
}

DoublyLinkedList *get_expression_uplinks(Expression *expression) {
    switch (expression->type) {
        case (VAR_EXPRESSION):
            return expression->value.var.uplinks;
        case (LAMBDA_EXPRESSION):
            return expression->value.lambda.uplinks;
        case (APP_EXPRESSION):
            return expression->value.app.uplinks;
        case (FORALL_EXPRESSION):
            return expression->value.forall.uplinks;
        case (TYPE_EXPRESSION):
            return expression->value.type.uplinks;
        case (PROP_EXPRESSION):
            return expression->value.prop.uplinks;
        case (HOLE_EXPRESSION):
            return expression->value.hole.uplinks;
        default:
            fprintf(
                stderr, ERROR
                "Unknown expression type in get_expression_uplinks.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

Expression *get_expression_type(Expression *expression) {
    switch (expression->type) {
        case (VAR_EXPRESSION):
            return expression->value.var.type;
        case (LAMBDA_EXPRESSION):
            return expression->value.lambda.type;
        case (APP_EXPRESSION):
            return expression->value.app.type;
        case (FORALL_EXPRESSION):
            return expression->value.forall.type;
        case (TYPE_EXPRESSION):
            return expression;
        case (PROP_EXPRESSION):
            return init_type_expression();
        case (HOLE_EXPRESSION):
            return expression->value.hole.type;
    }
}

Expression *get_expression_body(Expression *expression) {
    if (expression->type != VAR_EXPRESSION) {
        return NULL;
    }

    return expression->value.var.definition;
}

Context *get_expression_context(Expression *expression) {
    switch (expression->type) {
        case (VAR_EXPRESSION):
            return expression->value.var.context;
        case (LAMBDA_EXPRESSION):
            return expression->value.lambda.context;
        case (APP_EXPRESSION):
            return expression->value.app.context;
        case (FORALL_EXPRESSION):
            return expression->value.forall.context;
        case (TYPE_EXPRESSION):
            return context_create_empty();
        case (PROP_EXPRESSION):
            return context_create_empty();
        case (HOLE_EXPRESSION):
            return expression->value.hole.context;
    }
}

Expression *get_innermost_body(Expression *e) {
    if (e->type == LAMBDA_EXPRESSION) {
        return get_innermost_body(e->value.lambda.body);
    }
    if (e->type == FORALL_EXPRESSION) {
        return get_innermost_body(e->value.forall.body);
    }
    return e;
}

Expression *get_innermost_func(Expression *e) {
    if (e->type == APP_EXPRESSION) {
        return get_innermost_func(e->value.app.func);
    }
    return e;
}

Expression *get_app_func(Expression *expr) {
    if (expr->type != APP_EXPRESSION) {
        return NULL;
    }

    return expr->value.app.func;
}

Expression *get_app_arg(Expression *expr) {
    if (expr->type != APP_EXPRESSION) {
        return NULL;
    }

    return expr->value.app.arg;
}

Expression *get_forall_bound_variable(Expression *expr) {
    if (expr->type != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->value.forall.bound_variable;
}

Expression *get_lambda_bound_variable(Expression *expr) {
    if (expr->type != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->value.lambda.bound_variable;
}

Expression *get_forall_body(Expression *expr) {
    if (expr->type != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->value.forall.body;
}

Expression *get_lambda_body(Expression *expr) {
    if (expr->type != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->value.lambda.body;
}

Expression *get_arrow_lhs(Expression *expr) {
    if (expr->type != FORALL_EXPRESSION) {
        return NULL;
    }

    return get_expression_type(get_forall_bound_variable(expr));
}

Expression *get_arrow_rhs(Expression *expr) {
    if (expr->type != FORALL_EXPRESSION) {
        return NULL;
    }

    return get_forall_body(expr);
}

// Forward declarations. No need to expose them in expression.h.
void free_var_expression(Expression *expr);
void free_lambda_expression(Expression *expr);
void free_app_expression(Expression *expr);
void free_forall_expression(Expression *expr);
void free_type_expression(Expression *expr);
void free_prop_expression(Expression *expr);
void free_hole_expression(Expression *expr);

void free_expression(Expression *expr) {
    switch (expr->type) {
        case (VAR_EXPRESSION):
            free_var_expression(expr);
            break;
        case (LAMBDA_EXPRESSION):
            free_lambda_expression(expr);
            break;
        case (APP_EXPRESSION):
            free_app_expression(expr);
            break;
        case (FORALL_EXPRESSION):
            free_forall_expression(expr);
            break;
        case (TYPE_EXPRESSION):
            free_type_expression(expr);
            break;
        case (PROP_EXPRESSION):
            free_prop_expression(expr);
            break;
        case (HOLE_EXPRESSION):
            free_hole_expression(expr);
            break;
        default:
            break;
    }
}

void free_var_expression(Expression *expr) {
    if (expr && expr->type == VAR_EXPRESSION) {
        context_free(expr->value.var.context);
        free(expr->value.var.name);
        dll_destroy(expr->value.var.uplinks);
        free(expr);
    }
}

void free_lambda_expression(Expression *expr) {
    if (expr && expr->type == LAMBDA_EXPRESSION) {
        context_free(expr->value.lambda.context);
        free_expression(expr->value.lambda.type);
        dll_destroy(expr->value.lambda.uplinks);
        free(expr);
    }
}

void free_app_expression(Expression *expr) {
    if (expr && expr->type == APP_EXPRESSION) {
        if (expr->value.app.cache) {
            free_expression(expr->value.app.cache);
        }
        free_expression(expr->value.app.type);
        context_free(expr->value.app.context);
        dll_destroy(expr->value.app.uplinks);
        free(expr);
    }
}

void free_forall_expression(Expression *expr) {
    if (expr && expr->type == FORALL_EXPRESSION) {
        context_free(expr->value.forall.context);
        free_expression(expr->value.forall.type);
        free_expression(expr->value.forall.body);
        dll_destroy(expr->value.forall.uplinks);
        free(expr);
    }
}

void free_type_expression(Expression *expr) {
    if (expr && expr->type == TYPE_EXPRESSION) {
        dll_destroy(expr->value.type.uplinks);
        free(expr);
    }
}

void free_prop_expression(Expression *expr) {
    if (expr && expr->type == PROP_EXPRESSION) {
        dll_destroy(expr->value.type.uplinks);
        free(expr);
    }
}

void free_hole_expression(Expression *expr) {
    if (expr && expr->type == HOLE_EXPRESSION) {
        free(expr->value.hole.name);
        free_expression(expr->value.hole.type);
        context_free(expr->value.hole.context);
        dll_destroy(expr->value.forall.uplinks);
        free(expr);
    }
}

bool _congruence(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return true;
    }

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return _congruence(a->value.app.func, b->value.app.func, mapping) &&
                   _congruence(a->value.app.arg, b->value.app.arg, mapping);
        case (FORALL_EXPRESSION): {
            map_set(mapping, a->value.forall.bound_variable,
                    b->value.forall.bound_variable);
            return _congruence(a->value.forall.body, b->value.forall.body,
                               mapping);
        }
        case (LAMBDA_EXPRESSION): {
            map_set(mapping, a->value.lambda.bound_variable,
                    b->value.lambda.bound_variable);
            return _congruence(a->value.lambda.body, b->value.lambda.body,
                               mapping);
        }
        case (VAR_EXPRESSION): {
            return (a == b) || (map_get(mapping, a) == b);
        }
        case (HOLE_EXPRESSION): {
            return (a == b) || (map_get(mapping, a) == b);
        }
    }
}

bool congruence(Expression *a, Expression *b) {
    Map *mapping = map_new();
    bool result = _congruence(a, b, mapping);
    free(mapping->items);
    free(mapping);
    return result;
}

bool subtypes(Expression *a, Expression *b) {
    // We don't implement a full subtyping relation, but it is necessary
    // specifically for Type and Prop.
    if (a->type == PROP_EXPRESSION && b->type == TYPE_EXPRESSION) {
        return true;
    }

    return congruence(a, b);
}

void _match_and_subst(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return;
    }

    switch (a->type) {
        case (TYPE_EXPRESSION):
            break;
        case (PROP_EXPRESSION):
            break;
        case (APP_EXPRESSION):
            _match_and_subst(a->value.app.func, b->value.app.func, mapping);
            _match_and_subst(a->value.app.arg, b->value.app.arg, mapping);
            break;
        case (FORALL_EXPRESSION): {
            map_set(mapping, a->value.forall.bound_variable,
                    b->value.forall.bound_variable);
            _match_and_subst(a->value.forall.body, b->value.forall.body,
                             mapping);
            break;
        }
        case (LAMBDA_EXPRESSION): {
            map_set(mapping, a->value.lambda.bound_variable,
                    b->value.lambda.bound_variable);
            _match_and_subst(a->value.lambda.body, b->value.lambda.body,
                             mapping);
            break;
        }
        case (VAR_EXPRESSION): {
            if (a != b) {
                (map_set(mapping, a, b));
            }
            break;
        }
        case (HOLE_EXPRESSION): {
            if (a != b) {
                (map_set(mapping, a, b));
            }
            break;
        }
    }
}

Expression *match_and_subst(Expression *a, Expression *b,
                            Expression *to_subst) {
    Map *mapping = map_new();
    _match_and_subst(a, b, mapping);

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    int n = mapping->size;
    for (int i = 0; i < n; i++) {
        dll_insert_at_tail(old_exprs, dll_new_node((mapping->items + i)->key));
        dll_insert_at_tail(new_exprs, dll_new_node((mapping->items + i)->val));
    }

    Expression *result = new_p_subst(to_subst, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    free(mapping->items);
    free(mapping);
    return result;
}

bool _congruent_with_holes(Expression *a, Expression *b,
                           Map *alpha_equivalences, Map *required_holes) {
    if (a == b) {
        return true;
    }

    if (a->type == HOLE_EXPRESSION && b->type == HOLE_EXPRESSION) {
        return false;  // TODO: what do we do in this case?
    }
    if (a->type == HOLE_EXPRESSION) {
        bool b_can_fill = can_fill(a, b);
        if (b_can_fill) {
            map_set(required_holes, a, b);
            return true;
        }
        return false;
    }
    if (b->type == HOLE_EXPRESSION) {
        bool a_can_fill = can_fill(b, a);
        if (a_can_fill) {
            map_set(required_holes, b, a);
            return true;
        }
        return false;
    }

    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION): {
            bool result1 =
                _congruent_with_holes(a->value.app.func, b->value.app.func,
                                      alpha_equivalences, required_holes);
            bool result2 =
                _congruent_with_holes(a->value.app.arg, b->value.app.arg,
                                      alpha_equivalences, required_holes);
            return result1 && result2;
        }
        case (FORALL_EXPRESSION): {
            map_set(alpha_equivalences, a->value.forall.bound_variable,
                    b->value.forall.bound_variable);
            bool result = _congruent_with_holes(
                a->value.forall.body, b->value.forall.body, alpha_equivalences,
                required_holes);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            map_set(alpha_equivalences, a->value.lambda.bound_variable,
                    b->value.lambda.bound_variable);
            bool result = _congruent_with_holes(
                a->value.lambda.body, b->value.lambda.body, alpha_equivalences,
                required_holes);
            return result;
        }
        case (VAR_EXPRESSION): {
            return (a == b) || map_get(alpha_equivalences, a) == b;
        }
        default:
            return false;
    }
}

bool congruent_with_holes(Expression *a, Expression *b) {
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool result =
        _congruent_with_holes(a, b, alpha_equivalences, required_holes);
    map_clear_free(alpha_equivalences);
    map_clear_free(required_holes);
    return result;
}

bool get_maybe_hole_free(Expression *expr) {
    switch (expr->type) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return expr->value.app.maybe_hole_free;
        case (FORALL_EXPRESSION):
            return expr->value.forall.maybe_hole_free;
        case (LAMBDA_EXPRESSION):
            return expr->value.lambda.maybe_hole_free;
        case (VAR_EXPRESSION):
            return expr->value.var.maybe_hole_free;
        default:
            return false;
    }
}

bool has_holes(Expression *expr) {
    if (get_maybe_hole_free(expr)) {
        return false;
    }

    switch (expr->type) {
        case (TYPE_EXPRESSION):
        case (PROP_EXPRESSION):
            return false;
        case (HOLE_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return has_holes(expr->value.app.func) ||
                   has_holes(expr->value.app.arg);
        case (FORALL_EXPRESSION):
            return has_holes(expr->value.forall.body);
        case (LAMBDA_EXPRESSION):
            return has_holes(expr->value.lambda.body);
        case (VAR_EXPRESSION):
            return false;
        default:
            fprintf(stderr,
                    ERROR "Unknown expression type in has_holes.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

bool is_hole(Expression *expr) { return expr->type == HOLE_EXPRESSION; }

// Returns true if you can safely substitute term into a hole.
// This means three things:
//    1) The type(term) == expected return type of hole.
//    2) The defining context of hole contains the context(term).
//    3) Term does not itself contain the hole.
// This does no modifications/creates no new objects.
bool can_fill(Expression *hole, Expression *term) {
    bool types_match = congruent_with_holes(get_expression_type(hole),
                                            get_expression_type(term));
    if (get_maybe_hole_free(term)) {
        return types_match &&
               valid_in_context(term, get_expression_context(hole));
    }
    bool occurs = occurs_in(hole, term);
    return types_match &&
           valid_in_context(term, get_expression_context(hole)) && !occurs;
}

bool _occurs_in(Expression *var_or_hole, Expression *term, Map *visited) {
    if (map_get(visited, term) != NULL) {
        return false;
    }
    map_set(visited, term, term);

    if (var_or_hole == term) {
        return true;
    }

    switch (term->type) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return false;
        case VAR_EXPRESSION:
            return var_or_hole == term;
        case APP_EXPRESSION:
            return _occurs_in(var_or_hole, term->value.app.func, visited) ||
                   _occurs_in(var_or_hole, term->value.app.arg, visited);
        case LAMBDA_EXPRESSION:
            return _occurs_in(var_or_hole, term->value.lambda.bound_variable,
                              visited) ||
                   _occurs_in(var_or_hole, term->value.lambda.body, visited);
        case FORALL_EXPRESSION:
            return _occurs_in(var_or_hole, term->value.forall.bound_variable,
                              visited) ||
                   _occurs_in(var_or_hole, term->value.forall.body, visited);
        case HOLE_EXPRESSION:
            return var_or_hole == term;
        default:
            fprintf(stderr,
                    ERROR "Unknown expression type in occurs_in.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

bool occurs_in(Expression *var_or_hole, Expression *term) {
    return _occurs_in(var_or_hole, term, map_new());
}

void fill_hole(Expression *hole, Expression *term) {
    if (hole->type != HOLE_EXPRESSION) {
        return;
    }

    if (occurs_in(hole, term)) {
        return;
    }

    // check if term satisfies hole type...
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool types_match = _congruent_with_holes(
        get_expression_type(hole), get_expression_type(term),
        alpha_equivalences, required_holes);
    if (!types_match) {
        map_clear_free(alpha_equivalences);
        map_clear_free(required_holes);
        return;  // Todo: signal that this has failed?
    }

    int n = required_holes->size;
    for (int i = 0; i < n; i++) {
        Expression *hole = (required_holes->items + i)->key;
        Expression *substitute = (required_holes->items + i)->val;
        fill_hole(hole, substitute);
    }

    DoublyLinkedList *holepars = hole->value.hole.uplinks;
    for (int i = 0; i < dll_len(holepars); i++) {
        Uplink *uplink = dll_at(holepars, i)->data;
        switch (uplink->relation) {
            case (LAMBDA_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.lambda.body = term;
                break;
            }
            case (APP_FUNC): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.app.func = term;
                break;
            }
            case (APP_ARG): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.app.arg = term;
                break;
            }
            case (FORALL_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.forall.body = term;
                break;
            }
            case (CTX_VAR): {
                Context *ptr = (Context *)uplink->ptr;
                ptr->var_type = term;
                break;
            }
            case (HOLE_TYPE): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.hole.type = term;
                break;
            }
            case (VAR_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->value.var.definition = term;
            }
            default:
                break;
        }
    }

    map_clear_free(alpha_equivalences);
    map_clear_free(required_holes);
}

char c_counter = 'a';
char *get_char() {
    char temp[2] = {c_counter, '\0'};
    c_counter += 1;
    if (c_counter > 'z') {
        c_counter = 'a';
    }
    return strdup(temp);
}

bool _congruence2(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return true;
    }

    if (a->type == b->type) {
        switch (a->type) {
            case (TYPE_EXPRESSION):
                return true;
            case (PROP_EXPRESSION):
                return true;
            case (APP_EXPRESSION):
                return _congruence2(a->value.app.func, b->value.app.func,
                                    mapping) &&
                       _congruence2(a->value.app.arg, b->value.app.arg,
                                    mapping);
            case (FORALL_EXPRESSION): {
                map_set(mapping, a->value.forall.bound_variable,
                        b->value.forall.bound_variable);
                return _congruence2(a->value.forall.body, b->value.forall.body,
                                    mapping);
            }
            case (LAMBDA_EXPRESSION): {
                map_set(mapping, a->value.lambda.bound_variable,
                        b->value.lambda.bound_variable);
                return _congruence2(a->value.lambda.body, b->value.lambda.body,
                                    mapping);
            }
            case (VAR_EXPRESSION): {
                return (a == b) || (map_get(mapping, a) == b);
            }
            case (HOLE_EXPRESSION): {
                return (a == b) || (map_get(mapping, a) == b);
            }
        }
    } else {
        if (a->type == HOLE_EXPRESSION || b->type == HOLE_EXPRESSION) {
            map_set(mapping, a, b);
            return true;
        }
    }

    return false;
}

bool congruence2(Expression *a, Expression *b) {
    Map *mapping = map_new();
    bool result = _congruence2(a, b, mapping);
    free(mapping->items);
    free(mapping);
    return result;
}