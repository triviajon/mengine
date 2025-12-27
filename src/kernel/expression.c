#include "src/kernel/expression.h"

#include <stdio.h>

#include "src/common/color.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/dyn_array_map.h"
#include "src/kernel/new_subst.h"

void add_to_parents(Expression *expression, void *ptr, Relation r) {
    Uplink *uplink = new_uplink(ptr, r);
    dll_insert_at_head(expression->uplinks, dll_new_node(uplink));
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
Expression *_construct_lambda_type(Expression *bound_variable, Expression *body) {
    return init_forall_expression_wc(bound_variable, get_expression_type(body));
}

// Helper to construct a app type from a function and argument.
// Assumes all inputs are valid.
Expression *_construct_app_type(Context *context, Expression *func, Expression *arg) {
    Expression *func_type = get_expression_type(func);  // Forall x: A, B
    Expression *weak_func_type = weak_head_normalize(func_type);
    if (weak_func_type->tag != FORALL_EXPRESSION) {
        fprintf(stderr, ERROR "Trying to apply a non-function.\n" CRESET);
        return NULL;
    }
    Expression *variable = get_forall_bound_variable(weak_func_type);  // x
    Expression *expected_arg_type = get_expression_type(variable);     // A
    Expression *actual_arg_type = get_expression_type(arg);            // A?
    Expression *return_type = get_forall_body(weak_func_type);         // B

    if (subtypes(actual_arg_type, expected_arg_type)) {
        // return_type (B) is closed under context(variable) extended with variable
        // context must include both variable and all of arg's dependencies
        return new_subst(context, return_type, variable, arg);  // B[x -> arg]
    }

    fprintf(stderr, ERROR "Application does not type check.\n" CRESET);
    return NULL;
}

Expression *_init_expression_base(ExpressionType tag, Context *context, int ctx_size,
                                  Expression *type, bool maybe_hole_free) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    if (!expr) {
        return NULL;
    }

    SET_EXPR_TAG(expr, tag);
    SET_EXPR_UPLINKS(expr, dll_create());
    SET_EXPR_CONTEXT(expr, context);
    SET_EXPR_CTX_SIZE(expr, ctx_size);
    SET_EXPR_TYPE(expr, type);
    SET_EXPR_MAYBE_HOLE_FREE(expr, maybe_hole_free);

    return expr;
}

Expression *init_prop_expression() {
    if (PROP == NULL) {
        PROP =
            _init_expression_base(/* tag */ PROP_EXPRESSION, /* context */ context_create_empty(),
                                  /* ctx_size */ 0, /* type */ init_type_expression(),
                                  /* maybe_hole_free */ true);
    }
    return PROP;
}

Expression *init_type_expression() {
    if (TYPE == NULL) {
        Context *context = context_create_empty();
        // Special case: Type's type is recursive, so we need to create it manually.
        TYPE = malloc(sizeof(Expression));
        if (!TYPE) {
            return NULL;
        }
        SET_EXPR_TAG(TYPE, TYPE_EXPRESSION);
        SET_EXPR_UPLINKS(TYPE, dll_create());
        SET_EXPR_CONTEXT(TYPE, context);
        SET_EXPR_CTX_SIZE(TYPE, 0);
        SET_EXPR_TYPE(TYPE, init_type_expression());
    }
    return TYPE;
}

Expression *init_hole_expression(char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ HOLE_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ type,
                                             /* maybe_hole_free */ false);

    SET_HOLE_NAME(expr, name);
    return expr;
}

Expression *init_var_expression_wc(const char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ VAR_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size + 1,
                                             /* type */ type,
                                             /* maybe_hole_free */ true);

    SET_VAR_NAME(expr, strdup(name));
    return expr;
}

Expression *init_var_expression_wc_with_body(const char *name, Expression *body, Context *gamma) {
    if (!valid_in_context(body, gamma)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type = get_expression_type(body);
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ VAR_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size + 1,
                                             /* type */ type,
                                             /* maybe_hole_free */ true);

    SET_VAR_NAME(expr, strdup(name));
    SET_VAR_BODY(expr, body);

    return expr;
}

Expression *init_lambda_expression_wc(Expression *bound_variable, Expression *body) {
    Context *gamma = get_expression_context(bound_variable);
    Context *extended_with_bound_variable = bound_variable;

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr =
        _init_expression_base(/* tag */ LAMBDA_EXPRESSION, /* context */ gamma,
                              /* ctx_size */ gamma->ctx_size,
                              /* type */ _construct_lambda_type(bound_variable, body),
                              /* maybe_hole_free */ get_maybe_hole_free(body));

    SET_LAMBDA_BOUND_VAR(expr, bound_variable);
    SET_LAMBDA_BODY(expr, body);

    return expr;
}

Expression *init_app_expression_wc(Expression *func, Expression *arg, Context *context) {
    if (!valid_in_context(func, context)) {
        fprintf(stderr, ERROR "Function is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *func_type = get_expression_type(func);
    if (func_type->tag != FORALL_EXPRESSION) {
        fprintf(stderr, ERROR "Function is not a Forall expression.\n" CRESET);
        return NULL;
    }

    if (!valid_in_context(arg, context)) {
        fprintf(stderr, ERROR "Argument is not valid in context.\n" CRESET);
        return NULL;
    }

    // We perform the verification that type of the argument is a subtype of the
    // function's bound variable type in the _construct_app_type helper.
    Expression *type = _construct_app_type(context, func, arg);
    if (!type) {
        return NULL;
    }

    Expression *expr = _init_expression_base(
        /* tag */ APP_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ type,
        /* maybe_hole_free */ get_maybe_hole_free(func) && get_maybe_hole_free(arg));

    SET_APP_FUNC(expr, func);
    SET_APP_ARG(expr, arg);

    return expr;
}

Expression *init_forall_expression_wc(Expression *bound_variable, Expression *body) {
    Context *gamma = get_expression_context(bound_variable);
    Context *extended_with_bound_variable = bound_variable;

    if (!valid_in_context(body, extended_with_bound_variable)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *body_type = get_expression_type(body);
    if (body_type->tag != PROP_EXPRESSION && body_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Body type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *bound_variable_type = get_expression_type(bound_variable);
    if (body_type->tag == TYPE_EXPRESSION && !valid_in_context(bound_variable_type, gamma)) {
        fprintf(stderr, ERROR "Bound variable type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ FORALL_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ body_type,
                                             /* maybe_hole_free */ get_maybe_hole_free(body));

    SET_FORALL_BOUND_VAR(expr, bound_variable);
    SET_FORALL_BODY(expr, body);

    return expr;
}

Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs, Context *gamma) {
    // lhs -> rhs <-> Forall _: lhs, rhs
    Expression *unnamed_variable = init_var_expression_wc("_", lhs, gamma);
    return init_forall_expression_wc(unnamed_variable, rhs);
}

DoublyLinkedList *get_expression_uplinks(Expression *expression) {
    switch (expression->tag) {
        case (VAR_EXPRESSION):
            return expression->uplinks;
        case (LAMBDA_EXPRESSION):
            return expression->uplinks;
        case (APP_EXPRESSION):
            return expression->uplinks;
        case (FORALL_EXPRESSION):
            return expression->uplinks;
        case (TYPE_EXPRESSION):
            return expression->uplinks;
        case (PROP_EXPRESSION):
            return expression->uplinks;
        case (HOLE_EXPRESSION):
            return expression->uplinks;
        default:
            fprintf(stderr, ERROR "Unknown expression type in get_expression_uplinks.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

Expression *get_expression_type(Expression *expression) { return expression->type; }

Expression *get_expression_body(Expression *expression) {
    if (expression->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expression->as.var.body;
}

Context *get_expression_context(Expression *expression) { return expression->context; }

Expression *get_innermost_body(Expression *e) {
    if (e->tag == LAMBDA_EXPRESSION) {
        return get_innermost_body(e->as.lambda.body);
    }
    if (e->tag == FORALL_EXPRESSION) {
        return get_innermost_body(e->as.forall.body);
    }
    return e;
}

Expression *get_innermost_func(Expression *e) {
    if (e->tag == APP_EXPRESSION) {
        return get_innermost_func(e->as.app.func);
    }
    return e;
}

char *get_var_name(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expr->as.var.name;
}

char *get_hole_name(Expression *expr) {
    if (expr->tag != HOLE_EXPRESSION) {
        return NULL;
    }

    return expr->as.hole.name;
}

Expression *get_app_func(Expression *expr) {
    if (expr->tag != APP_EXPRESSION) {
        return NULL;
    }

    return expr->as.app.func;
}

Expression *get_app_arg(Expression *expr) {
    if (expr->tag != APP_EXPRESSION) {
        return NULL;
    }

    return expr->as.app.arg;
}

Expression *get_forall_bound_variable(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->as.forall.bound_variable;
}

Expression *get_lambda_bound_variable(Expression *expr) {
    if (expr->tag != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->as.lambda.bound_variable;
}

Expression *get_forall_body(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return expr->as.forall.body;
}

Expression *get_lambda_body(Expression *expr) {
    if (expr->tag != LAMBDA_EXPRESSION) {
        return NULL;
    }

    return expr->as.lambda.body;
}

Expression *get_arrow_lhs(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
        return NULL;
    }

    return get_expression_type(get_forall_bound_variable(expr));
}

Expression *get_arrow_rhs(Expression *expr) {
    if (expr->tag != FORALL_EXPRESSION) {
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
    switch (expr->tag) {
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
            fprintf(stderr, ERROR "Unknown expression type in free_expression.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

void free_var_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_lambda_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_app_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_forall_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_type_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_prop_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

void free_hole_expression(Expression *expr) {
    (void)expr;
    // todo: implement me!
}

bool _congruence(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return true;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return _congruence(a->as.app.func, b->as.app.func, mapping) &&
                   _congruence(a->as.app.arg, b->as.app.arg, mapping);
        case (FORALL_EXPRESSION): {
            map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            return _congruence(a->as.forall.body, b->as.forall.body, mapping);
        }
        case (LAMBDA_EXPRESSION): {
            map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            return _congruence(a->as.lambda.body, b->as.lambda.body, mapping);
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
    if (a->tag == PROP_EXPRESSION && b->tag == TYPE_EXPRESSION) {
        return true;
    }

    return congruence(a, b);
}

void _match_and_subst(Expression *a, Expression *b, Map *mapping) {
    // Mapping is a map from variables in a to variables in b.
    if (a == b) {
        return;
    }

    switch (a->tag) {
        case (TYPE_EXPRESSION):
            break;
        case (PROP_EXPRESSION):
            break;
        case (APP_EXPRESSION):
            _match_and_subst(a->as.app.func, b->as.app.func, mapping);
            _match_and_subst(a->as.app.arg, b->as.app.arg, mapping);
            break;
        case (FORALL_EXPRESSION): {
            map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            _match_and_subst(a->as.forall.body, b->as.forall.body, mapping);
            break;
        }
        case (LAMBDA_EXPRESSION): {
            map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            _match_and_subst(a->as.lambda.body, b->as.lambda.body, mapping);
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

Expression *match_and_subst(Expression *a, Expression *b, Expression *to_subst) {
    Map *mapping = map_new();
    _match_and_subst(a, b, mapping);

    DoublyLinkedList *old_exprs = dll_create();
    DoublyLinkedList *new_exprs = dll_create();

    int n = mapping->size;
    for (int i = 0; i < n; i++) {
        dll_insert_at_tail(old_exprs, dll_new_node((mapping->items + i)->key));
        dll_insert_at_tail(new_exprs, dll_new_node((mapping->items + i)->val));
    }

    Context *to_subst_ctx = get_expression_context(to_subst);
    Expression *result = new_p_subst(to_subst_ctx, to_subst, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    free(mapping->items);
    free(mapping);
    return result;
}

bool _congruent_with_holes(Expression *a, Expression *b, Map *alpha_equivalences,
                           Map *required_holes) {
    if (a == b) {
        return true;
    }

    if (a->tag == HOLE_EXPRESSION && b->tag == HOLE_EXPRESSION) {
        return false;  // TODO: what do we do in this case?
    }
    if (a->tag == HOLE_EXPRESSION) {
        bool b_can_fill = can_fill(a, b);
        if (b_can_fill) {
            map_set(required_holes, a, b);
            return true;
        }
        return false;
    }
    if (b->tag == HOLE_EXPRESSION) {
        bool a_can_fill = can_fill(b, a);
        if (a_can_fill) {
            map_set(required_holes, b, a);
            return true;
        }
        return false;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
        case (TYPE_EXPRESSION):
            return true;
        case (PROP_EXPRESSION):
            return true;
        case (APP_EXPRESSION): {
            bool result1 = _congruent_with_holes(a->as.app.func, b->as.app.func, alpha_equivalences,
                                                 required_holes);
            bool result2 = _congruent_with_holes(a->as.app.arg, b->as.app.arg, alpha_equivalences,
                                                 required_holes);
            return result1 && result2;
        }
        case (FORALL_EXPRESSION): {
            map_set(alpha_equivalences, a->as.forall.bound_variable, b->as.forall.bound_variable);
            bool result = _congruent_with_holes(a->as.forall.body, b->as.forall.body,
                                                alpha_equivalences, required_holes);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            map_set(alpha_equivalences, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            bool result = _congruent_with_holes(a->as.lambda.body, b->as.lambda.body,
                                                alpha_equivalences, required_holes);
            return result;
        }
        case (VAR_EXPRESSION): {
            return (a == b) || map_get(alpha_equivalences, a) == b;
        }
        default:
            fprintf(stderr, ERROR "Unknown expression type in _congruent_with_holes.\n" CRESET);
            return false;
    }
}

bool congruent_with_holes(Expression *a, Expression *b) {
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool result = _congruent_with_holes(a, b, alpha_equivalences, required_holes);
    map_clear_free(alpha_equivalences);
    map_clear_free(required_holes);
    return result;
}

bool get_maybe_hole_free(Expression *expr) { return expr->maybe_hole_free; }

bool has_holes(Expression *expr) {
    if (get_maybe_hole_free(expr)) {
        return false;
    }

    switch (expr->tag) {
        case (TYPE_EXPRESSION):
        case (PROP_EXPRESSION):
            return false;
        case (HOLE_EXPRESSION):
            return true;
        case (APP_EXPRESSION):
            return has_holes(expr->as.app.func) || has_holes(expr->as.app.arg);
        case (FORALL_EXPRESSION):
            return has_holes(expr->as.forall.body);
        case (LAMBDA_EXPRESSION):
            return has_holes(expr->as.lambda.body);
        case (VAR_EXPRESSION):
            return false;
        default:
            fprintf(stderr, ERROR "Unknown expression type in has_holes.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

bool is_hole(Expression *expr) { return expr->tag == HOLE_EXPRESSION; }

// Returns true if you can safely substitute term into a hole.
// This means three things:
//    1) The type(term) == expected return type of hole.
//    2) The defining context of hole contains the context(term).
//    3) Term does not itself contain the hole.
// This does no modifications/creates no new objects.
bool can_fill(Expression *hole, Expression *term) {
    bool types_match = congruent_with_holes(get_expression_type(hole), get_expression_type(term));
    if (get_maybe_hole_free(term)) {
        return types_match && valid_in_context(term, get_expression_context(hole));
    }
    bool occurs = occurs_in(hole, term);
    return types_match && valid_in_context(term, get_expression_context(hole)) && !occurs;
}

bool _occurs_in(Expression *var_or_hole, Expression *term, Map *visited) {
    if (map_get(visited, term) != NULL) {
        return false;
    }
    map_set(visited, term, term);

    if (var_or_hole == term) {
        return true;
    }

    switch (term->tag) {
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            return false;
        case VAR_EXPRESSION:
            return var_or_hole == term;
        case APP_EXPRESSION:
            return _occurs_in(var_or_hole, term->as.app.func, visited) ||
                   _occurs_in(var_or_hole, term->as.app.arg, visited);
        case LAMBDA_EXPRESSION:
            return _occurs_in(var_or_hole, term->as.lambda.bound_variable, visited) ||
                   _occurs_in(var_or_hole, term->as.lambda.body, visited);
        case FORALL_EXPRESSION:
            return _occurs_in(var_or_hole, term->as.forall.bound_variable, visited) ||
                   _occurs_in(var_or_hole, term->as.forall.body, visited);
        case HOLE_EXPRESSION:
            return var_or_hole == term;
        default:
            fprintf(stderr, ERROR "Unknown expression type in occurs_in.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

bool occurs_in(Expression *var_or_hole, Expression *term) {
    return _occurs_in(var_or_hole, term, map_new());
}

void fill_hole(Expression *hole, Expression *term) {
    if (hole->tag != HOLE_EXPRESSION) {
        return;
    }

    if (occurs_in(hole, term)) {
        return;
    }

    // check if term satisfies hole type...
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool types_match = _congruent_with_holes(get_expression_type(hole), get_expression_type(term),
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

    DoublyLinkedList *holepars = hole->uplinks;
    for (int i = 0; i < dll_len(holepars); i++) {
        Uplink *uplink = dll_at(holepars, i)->data;
        switch (uplink->relation) {
            case (LAMBDA_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.lambda.body = term;
                break;
            }
            case (LAMBDA_BOUND_VAR): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.lambda.bound_variable = term;
                break;
            }
            case (APP_FUNC): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.app.func = term;
                break;
            }
            case (APP_ARG): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.app.arg = term;
                break;
            }
            case (FORALL_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.forall.body = term;
                break;
            }
            case (FORALL_BOUND_VAR): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.forall.bound_variable = term;
                break;
            }
            case (VAR_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->as.var.body = term;
                break;
            }
            case (EXPR_TYPE): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->type = term;
                break;
            }
            case (EXPR_CONTEXT): {
                Expression *ptr = (Expression *)uplink->ptr;
                ptr->context = term;
                break;
            }
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

    if (a->tag == b->tag) {
        switch (a->tag) {
            case (TYPE_EXPRESSION):
                return true;
            case (PROP_EXPRESSION):
                return true;
            case (APP_EXPRESSION):
                return _congruence2(a->as.app.func, b->as.app.func, mapping) &&
                       _congruence2(a->as.app.arg, b->as.app.arg, mapping);
            case (FORALL_EXPRESSION): {
                map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
                return _congruence2(a->as.forall.body, b->as.forall.body, mapping);
            }
            case (LAMBDA_EXPRESSION): {
                map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
                return _congruence2(a->as.lambda.body, b->as.lambda.body, mapping);
            }
            case (VAR_EXPRESSION): {
                return (a == b) || (map_get(mapping, a) == b);
            }
            case (HOLE_EXPRESSION): {
                return (a == b) || (map_get(mapping, a) == b);
            }
        }
    } else {
        if (a->tag == HOLE_EXPRESSION || b->tag == HOLE_EXPRESSION) {
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