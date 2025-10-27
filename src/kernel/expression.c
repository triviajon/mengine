#include "expression.h"

#include "axiom.h"
#include "beta_reduction.h"
#include "context.h"
#include "subst.h"

void add_to_parents(Expression *expression, Uplink *uplink) {
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
        case (FIX_EXPRESSION):
            dll_insert_at_head(expression->value.fix.uplinks,
                               dll_new_node(uplink));
            break;
        case (MATCH_EXPR_EXPRESSION):
            dll_insert_at_head(expression->value.matchExpr.uplinks,
                               dll_new_node(uplink));
            break;
        default:
            fprintf(stderr,
                    "Error: Unknown expression type in add_to_parents.\n");
            exit(EXIT_FAILURE);
    }
}

Uplink *new_uplink(Expression *parent, Relation relation) {
    Uplink *new_uplink = malloc(sizeof(Uplink));
    if (new_uplink == NULL) {
        return NULL;
    }
    new_uplink->expression = parent;
    new_uplink->relation = relation;
    return new_uplink;
}

Uplink *new_uplink2(Context *parent, Relation relation) {
    Uplink *new_uplink = malloc(sizeof(Uplink));
    if (new_uplink == NULL) {
        return NULL;
    }
    new_uplink->context = parent;
    new_uplink->relation = relation;
    return new_uplink;
}

// Helper function to construct a lambda type
Expression *constr_lambda_type(Expression *bound_variable, Expression *body) {
    Expression *type =
        init_forall_expression(bound_variable, get_expression_type(body));
    return type;
}

// Helper function to construct a app type
Expression *constr_app_type(Expression *func, Expression *arg) {
    Expression *func_type =
        get_expression_type(func);  // something like Forall x: A, B
    Expression *variable = func_type->value.forall.bound_variable;  // x
    Expression *expected_arg_type = get_expression_type(variable);  // A
    Expression *actual_arg_type =
        get_expression_type(arg);  // hopefully A, but we need to check.
    Expression *return_type = func_type->value.forall.body;  // B

    if (subtypes(actual_arg_type, expected_arg_type)) {
        return subst(return_type, variable, arg);  // return B[x -> arg]
    }

    // We if need to, normalize the arguments
    Expression *norm_actual_arg_ty = normalize(actual_arg_type);
    Expression *norm_expected_arg_ty = normalize(expected_arg_type);
    if (congruence(norm_actual_arg_ty, norm_expected_arg_ty)) {
        return subst(return_type, variable, arg);  // return B[x -> arg]
    }

    fprintf(stderr, "Error: Application does not type check.\n");
    exit(EXIT_FAILURE);
}

Expression *init_var_expression(const char *name, Expression *type) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.type = type;
    expr->value.var.uplinks = dll_create();
    expr->value.var.context =
        context_insert(get_expression_context(type), expr);
    return expr;
}

Expression *init_lambda_expression(Expression *bound_variable,
                                   Expression *body) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = LAMBDA_EXPRESSION;
    expr->value.lambda.context =
        context_minus(get_expression_context(body), bound_variable);
    expr->value.lambda.bound_variable = bound_variable;
    expr->value.lambda.type = constr_lambda_type(bound_variable, body);
    expr->value.lambda.body = body;
    add_to_parents(body, new_uplink(expr, LAMBDA_BODY));
    expr->value.lambda.uplinks = dll_create();
    return expr;
}

Expression *init_app_expression(Expression *func, Expression *arg) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = APP_EXPRESSION;
    Context *combined_ctx =
        context_add(get_expression_context(func), get_expression_context(arg));
    expr->value.app.context = combined_ctx;
    expr->value.app.func = func;
    add_to_parents(func, new_uplink(expr, APP_FUNC));
    expr->value.app.arg = arg;
    add_to_parents(arg, new_uplink(expr, APP_ARG));
    expr->value.app.type = constr_app_type(func, arg);
    expr->value.app.cache = NULL;
    expr->value.app.uplinks = dll_create();
    return expr;
}

Expression *init_forall_expression(Expression *bound_variable,
                                   Expression *body) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = FORALL_EXPRESSION;
    expr->value.forall.context =
        context_minus(context_add(get_expression_context(bound_variable),
                                  get_expression_context(body)),
                      bound_variable);
    expr->value.forall.bound_variable = bound_variable;
    // Set the type of the forall expression based on the type of its body
    Expression *body_type = get_expression_type(body);
    if (body_type == init_prop_expression()) {
        expr->value.forall.type = init_prop_expression();
    } else {
        expr->value.forall.type = init_type_expression();
    }
    expr->value.forall.body = body;
    add_to_parents(body, new_uplink(expr, FORALL_BODY));
    expr->value.forall.uplinks = dll_create();
    return expr;
}

Expression *init_prop_expression() {
    if (PROP == NULL) {
        PROP = (Expression *)malloc(sizeof(Expression));
        PROP->type = TYPE_EXPRESSION;
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

Expression *init_hole_expression(char *name, Expression *type,
                                 Context *context) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = HOLE_EXPRESSION;
    expr->value.hole.name = name;
    expr->value.hole.defining_context = context;
    expr->value.hole.return_type = type;
    add_to_parents(type, new_uplink(expr, HOLE_TYPE));
    expr->value.hole.uplinks = dll_create();
    return expr;
}

Expression *init_fix_expression(Expression *ident, Expression *bound_variable,
                                Expression *body) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = FIX_EXPRESSION;
    expr->value.fix.ident = ident;
    expr->value.fix.bound_variable = bound_variable;
    expr->value.fix.body = body;
    // TODO: wrong for similar reasons to match_expr.
    expr->value.fix.context = get_expression_context(body);
    expr->value.fix.type = constr_lambda_type(bound_variable, body);
    expr->value.fix.uplinks = dll_create();
    return expr;
}

Expression *init_match_expr_expression(
    Expression *match_scrutinee, Expression *literal_case_item,
    Expression *literal_result, Expression *var_case_item,
    Expression *var_result, Expression *op_case_item, Expression *op_result,
    Expression *type) {
    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = MATCH_EXPR_EXPRESSION;
    expr->value.matchExpr.match_scrutinee = get_innermost_body(match_scrutinee);
    expr->value.matchExpr.literal_case_item =
        get_innermost_body(literal_case_item);
    expr->value.matchExpr.literal_result = get_innermost_body(literal_result);
    expr->value.matchExpr.var_case_item = get_innermost_body(var_case_item);
    expr->value.matchExpr.var_result = get_innermost_body(var_result);
    expr->value.matchExpr.op_case_item = get_innermost_body(op_case_item);
    expr->value.matchExpr.op_result = get_innermost_body(op_result);
    expr->value.matchExpr.type = type;
    expr->value.matchExpr.context = context_add(
        context_add(context_add(get_expression_context(literal_case_item),
                                get_expression_context(literal_result)),
                    context_add(get_expression_context(var_case_item),
                                get_expression_context(var_result))),
        context_add(get_expression_context(op_case_item),
                    get_expression_context(op_result)));

    expr->value.matchExpr.uplinks = dll_create();
    return expr;
}

Expression *init_var_expression_wc(const char *name, Expression *type,
                                   Context *defining_context) {
    if (!valid_in_context(type, defining_context)) return NULL;

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = VAR_EXPRESSION;
    expr->value.var.name = strdup(name);
    expr->value.var.type = type;
    expr->value.var.uplinks = dll_create();
    expr->value.var.context = defining_context;
    return expr;
}

Expression *init_lambda_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *context) {
    if (!valid_in_context(body, context)) return NULL;

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = LAMBDA_EXPRESSION;
    expr->value.lambda.context = context_minus(context, bound_variable);
    expr->value.lambda.bound_variable = bound_variable;
    expr->value.lambda.type = constr_lambda_type(bound_variable, body);
    expr->value.lambda.body = body;
    add_to_parents(body, new_uplink(expr, LAMBDA_BODY));
    expr->value.lambda.uplinks = dll_create();
    return expr;
}

Expression *init_app_expression_wc(Expression *func, Expression *arg,
                                   Context *context) {
    if (!valid_in_context(func, context)) return NULL;
    if (!valid_in_context(arg, context)) return NULL;

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = APP_EXPRESSION;
    Context *combined_ctx = context;
    expr->value.app.context = combined_ctx;
    expr->value.app.func = func;
    add_to_parents(func, new_uplink(expr, APP_FUNC));
    expr->value.app.arg = arg;
    add_to_parents(arg, new_uplink(expr, APP_ARG));
    expr->value.app.type = constr_app_type(func, arg);
    expr->value.app.cache = NULL;
    expr->value.app.uplinks = dll_create();
    return expr;
}

Expression *init_forall_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *context) {
    if (!valid_in_context(body, context)) return NULL;

    Expression *expr = (Expression *)malloc(sizeof(Expression));
    expr->type = FORALL_EXPRESSION;
    expr->value.forall.context = context_minus(context, bound_variable);
    expr->value.forall.bound_variable = bound_variable;
    expr->value.forall.type = init_type_expression();
    expr->value.forall.body = body;
    add_to_parents(body, new_uplink(expr, FORALL_BODY));
    expr->value.forall.uplinks = dll_create();
    return expr;
}

Expression *extend_expression_context(Expression *expression,
                                      Expression *variable) {
    Context *original_context = get_expression_context(expression);
    if (!valid_to_add_to_context(variable, original_context)) return NULL;

    switch (expression->type) {
        case (VAR_EXPRESSION): {
            return init_var_expression_wc(
                expression->value.var.name, expression->value.var.type,
                context_insert(original_context, variable));
        }
        case (LAMBDA_EXPRESSION): {
            return init_lambda_expression_wc(
                expression->value.lambda.bound_variable,
                expression->value.lambda.body,
                context_insert(original_context, variable));
        }
        case (APP_EXPRESSION): {
            return init_app_expression_wc(
                expression->value.app.func, expression->value.app.arg,
                context_insert(original_context, variable));
        }
        case (FORALL_EXPRESSION): {
            return init_forall_expression_wc(
                expression->value.forall.bound_variable,
                expression->value.forall.body,
                context_insert(original_context, variable));
        }
        default:
            return NULL;
    }
}

Expression *init_arrow_expression(Expression *lhs, Expression *rhs) {
    // lhs -> rhs <-> Forall _: lhs, rhs
    Expression *unnamed_variable = init_var_expression("_", lhs);
    return init_forall_expression(unnamed_variable, rhs);
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
        case (FIX_EXPRESSION):
            return expression->value.fix.uplinks;
        case (MATCH_EXPR_EXPRESSION):
            return expression->value.matchExpr.uplinks;
        default:
            fprintf(
                stderr,
                "Error: Unknown expression type in get_expression_uplinks.\n");
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
            return expression->value.hole.return_type;
        case (FIX_EXPRESSION):
            return expression->value.fix.type;
        case (MATCH_EXPR_EXPRESSION):
            return expression->value.matchExpr.type;
    }
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
            return expression->value.hole.defining_context;
        case (MATCH_EXPR_EXPRESSION):
            return expression->value.matchExpr.context;
        case (FIX_EXPRESSION):
            return expression->value.fix.context;
    }
}

Expression *get_innermost_body(Expression *e) {
    if (e->type == LAMBDA_EXPRESSION) {
        return get_innermost_body(e->value.lambda.body);
    } else if (e->type == FORALL_EXPRESSION) {
        return get_innermost_body(e->value.forall.body);
    } else {
        return e;
    }
}

Expression *get_innermost_func(Expression *e) {
    if (e->type == APP_EXPRESSION) {
        return get_innermost_func(e->value.app.func);
    } else {
        return e;
    }
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
        free_expression(expr->value.hole.return_type);
        context_free(expr->value.hole.defining_context);
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
        case (FIX_EXPRESSION): {
            map_set(mapping, a->value.fix.ident, b->value.fix.ident);
            map_set(mapping, a->value.fix.bound_variable,
                    b->value.fix.bound_variable);
            return _congruence(a->value.fix.body, b->value.fix.body, mapping);
        }
        case (MATCH_EXPR_EXPRESSION): {
            map_set(mapping, a->value.matchExpr.match_scrutinee,
                    b->value.matchExpr.match_scrutinee);
            return _congruence(a->value.matchExpr.literal_case_item,
                               b->value.matchExpr.literal_case_item, mapping) &&
                   _congruence(a->value.matchExpr.literal_result,
                               b->value.matchExpr.literal_result, mapping) &&
                   _congruence(a->value.matchExpr.var_case_item,
                               b->value.matchExpr.var_case_item, mapping) &&
                   _congruence(a->value.matchExpr.var_result,
                               b->value.matchExpr.var_result, mapping) &&
                   _congruence(a->value.matchExpr.op_case_item,
                               b->value.matchExpr.op_case_item, mapping) &&
                   _congruence(a->value.matchExpr.op_result,
                               b->value.matchExpr.op_result, mapping) &&
                   _congruence(a->value.matchExpr.type, b->value.matchExpr.type,
                               mapping);
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
            if (a != b) (map_set(mapping, a, b));
            break;
        }
        case (HOLE_EXPRESSION): {
            if (a != b) (map_set(mapping, a, b));
            break;
        }
        case (FIX_EXPRESSION): {
            map_set(mapping, a->value.fix.ident, b->value.fix.ident);
            map_set(mapping, a->value.fix.bound_variable,
                    b->value.fix.bound_variable);
            _match_and_subst(a->value.fix.body, b->value.fix.body, mapping);
            break;
        }
        case (MATCH_EXPR_EXPRESSION): {
            map_set(mapping, a->value.matchExpr.match_scrutinee,
                    b->value.matchExpr.match_scrutinee);
            _match_and_subst(a->value.matchExpr.literal_case_item,
                             b->value.matchExpr.literal_case_item, mapping);
            _match_and_subst(a->value.matchExpr.literal_result,
                             b->value.matchExpr.literal_result, mapping);
            _match_and_subst(a->value.matchExpr.var_case_item,
                             b->value.matchExpr.var_case_item, mapping);
            _match_and_subst(a->value.matchExpr.var_result,
                             b->value.matchExpr.var_result, mapping);
            _match_and_subst(a->value.matchExpr.op_case_item,
                             b->value.matchExpr.op_case_item, mapping);
            _match_and_subst(a->value.matchExpr.op_result,
                             b->value.matchExpr.op_result, mapping);
            _match_and_subst(a->value.matchExpr.type, b->value.matchExpr.type,
                             mapping);
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

    Expression *result = p_subst(to_subst, old_exprs, new_exprs);

    dll_destroy(old_exprs);
    dll_destroy(new_exprs);
    free(mapping->items);
    free(mapping);
    return result;
}

bool _match_under_holes(Expression *a, Expression *b, Map *alpha_equivalences,
                        Map *required_holes) {
    if (a == b) {
        return true;
    }

    if (a->type == HOLE_EXPRESSION && b->type == HOLE_EXPRESSION) {
        return false;  // TODO: what do we do in this case?
    } else if (a->type == HOLE_EXPRESSION) {
        bool b_can_fill = can_fill(a, b);
        if (b_can_fill) {
            map_set(required_holes, a, b);
            return true;
        } else {
            return false;
        }
    } else if (b->type == HOLE_EXPRESSION) {
        bool a_can_fill = can_fill(b, a);
        if (a_can_fill) {
            map_set(required_holes, b, a);
            return true;
        } else {
            return false;
        }
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
                _match_under_holes(a->value.app.func, b->value.app.func,
                                   alpha_equivalences, required_holes);
            bool result2 =
                _match_under_holes(a->value.app.arg, b->value.app.arg,
                                   alpha_equivalences, required_holes);
            return result1 && result2;
        }
        case (FORALL_EXPRESSION): {
            map_set(alpha_equivalences, a->value.forall.bound_variable,
                    b->value.forall.bound_variable);
            bool result =
                _match_under_holes(a->value.forall.body, b->value.forall.body,
                                   alpha_equivalences, required_holes);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            map_set(alpha_equivalences, a->value.lambda.bound_variable,
                    b->value.lambda.bound_variable);
            bool result =
                _match_under_holes(a->value.lambda.body, b->value.lambda.body,
                                   alpha_equivalences, required_holes);
            return result;
        }
        case (VAR_EXPRESSION): {
            return (a == b) || map_get(alpha_equivalences, a) == b;
        }
        case (FIX_EXPRESSION): {
            map_set(alpha_equivalences, a->value.fix.ident, b->value.fix.ident);
            map_set(alpha_equivalences, a->value.fix.bound_variable,
                    b->value.fix.bound_variable);
            bool result =
                _match_under_holes(a->value.fix.body, b->value.fix.body,
                                   alpha_equivalences, required_holes);
            return result;
        }
        case (MATCH_EXPR_EXPRESSION): {
            map_set(alpha_equivalences, a->value.matchExpr.match_scrutinee,
                    b->value.matchExpr.match_scrutinee);
            bool result1 =
                _match_under_holes(a->value.matchExpr.literal_case_item,
                                   b->value.matchExpr.literal_case_item,
                                   alpha_equivalences, required_holes);
            bool result2 =
                _match_under_holes(a->value.matchExpr.literal_result,
                                   b->value.matchExpr.literal_result,
                                   alpha_equivalences, required_holes);
            bool result3 =
                _match_under_holes(a->value.matchExpr.var_case_item,
                                   b->value.matchExpr.var_case_item,
                                   alpha_equivalences, required_holes);
            bool result4 = _match_under_holes(
                a->value.matchExpr.var_result, b->value.matchExpr.var_result,
                alpha_equivalences, required_holes);
            bool result5 =
                _match_under_holes(a->value.matchExpr.op_case_item,
                                   b->value.matchExpr.op_case_item,
                                   alpha_equivalences, required_holes);
            bool result6 = _match_under_holes(
                a->value.matchExpr.op_result, b->value.matchExpr.op_result,
                alpha_equivalences, required_holes);
            bool result7 = _match_under_holes(
                a->value.matchExpr.type, b->value.matchExpr.type,
                alpha_equivalences, required_holes);
            return result1 && result2 && result3 && result4 && result5 &&
                   result6 && result7;
        }
        default:
            return false;
    }
}

bool has_holes(Expression *expr) {
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
        case (FIX_EXPRESSION):
            return has_holes(expr->value.fix.body);
        case (MATCH_EXPR_EXPRESSION):
            return has_holes(expr->value.matchExpr.match_scrutinee) ||
                   has_holes(expr->value.matchExpr.literal_case_item) ||
                   has_holes(expr->value.matchExpr.literal_result) ||
                   has_holes(expr->value.matchExpr.var_case_item) ||
                   has_holes(expr->value.matchExpr.var_result) ||
                   has_holes(expr->value.matchExpr.op_case_item) ||
                   has_holes(expr->value.matchExpr.op_result);
        default:
            fprintf(stderr, "Error: Unknown expression type in has_holes.\n");
            exit(EXIT_FAILURE);
    }
}

bool is_hole(Expression *expr) { return expr->type == HOLE_EXPRESSION; }

// Returns true if you can safely substitute term into a hole.
// This means two things:
//    1) The type(term) == expected return type of hole.
//    2) TODO: The defining context of hole contains the context(term).
// This does no modifications/creates no new objects.
bool can_fill(Expression *hole, Expression *term) {
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool types_match =
        _match_under_holes(get_expression_type(hole), get_expression_type(term),
                           alpha_equivalences, required_holes);
    map_clear_free(alpha_equivalences);
    map_clear_free(required_holes);
    return types_match && valid_in_context(term, get_expression_context(hole));
}

bool match_until_holes(Expression *with_holes, Expression *term) {
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool types_match = _match_under_holes(with_holes, term, alpha_equivalences,
                                          required_holes);
    map_clear_free(alpha_equivalences);
    map_clear_free(required_holes);
    return types_match;
}

void fillHole(Expression *hole, Expression *term) {
    if (hole->type != HOLE_EXPRESSION) {
        return;
    }

    // check if term satisfies hole type...
    Map *alpha_equivalences = map_new();
    Map *required_holes = map_new();
    bool types_match =
        _match_under_holes(get_expression_type(hole), get_expression_type(term),
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
        fillHole(hole, substitute);
    }

    DoublyLinkedList *holepars = hole->value.hole.uplinks;
    for (int i = 0; i < dll_len(holepars); i++) {
        Uplink *uplink = dll_at(holepars, i)->data;
        switch (uplink->relation) {
            case (APP_FUNC):
                uplink->expression->value.app.func = term;
                break;
            case (APP_ARG):
                uplink->expression->value.app.arg = term;
                break;
            case (LAMBDA_BODY):
                uplink->expression->value.lambda.body = term;
                break;
            case (HOLE_TYPE):
                uplink->expression->value.hole.return_type = term;
                break;
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
    if (c_counter > 'z') c_counter = 'a';
    return strdup(temp);
}

Expression *refresh(Expression *expr) {
    if (expr->type == LAMBDA_EXPRESSION) {
        Expression *x = expr->value.lambda.bound_variable;
        char *x_name = x->value.var.name;
        Expression *T = get_expression_type(x);
        Expression *B = expr->value.lambda.body;

        // char *xp_name = strcat(strdup(x_name), get_char());
        char *xp_name = strcat(strdup(x_name), "'");
        Expression *xp = init_var_expression(xp_name, T);
        return init_lambda_expression(xp, subst(B, x, xp));
    } else if (expr->type == FORALL_EXPRESSION) {
        Expression *x = expr->value.forall.bound_variable;
        char *x_name = x->value.var.name;
        Expression *T = get_expression_type(x);
        Expression *B = expr->value.forall.body;

        char *xp_name = strcat(strdup(x_name), get_char());

        Expression *xp = init_var_expression(xp_name, T);
        return init_forall_expression(xp, subst(B, x, xp));
    } else {
        return NULL;
    }
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
            case (FIX_EXPRESSION): {
                map_set(mapping, a->value.fix.ident, b->value.fix.ident);
                map_set(mapping, a->value.fix.bound_variable,
                        b->value.fix.bound_variable);
                return _congruence2(a->value.fix.body, b->value.fix.body,
                                    mapping);
            }
            case (MATCH_EXPR_EXPRESSION): {
                map_set(mapping, a->value.matchExpr.match_scrutinee,
                        b->value.matchExpr.match_scrutinee);
                return _congruence2(a->value.matchExpr.literal_case_item,
                                    b->value.matchExpr.literal_case_item,
                                    mapping) &&
                       _congruence2(a->value.matchExpr.literal_result,
                                    b->value.matchExpr.literal_result,
                                    mapping) &&
                       _congruence2(a->value.matchExpr.var_case_item,
                                    b->value.matchExpr.var_case_item,
                                    mapping) &&
                       _congruence2(a->value.matchExpr.var_result,
                                    b->value.matchExpr.var_result, mapping) &&
                       _congruence2(a->value.matchExpr.op_case_item,
                                    b->value.matchExpr.op_case_item, mapping) &&
                       _congruence2(a->value.matchExpr.op_result,
                                    b->value.matchExpr.op_result, mapping) &&
                       _congruence2(a->value.matchExpr.type,
                                    b->value.matchExpr.type, mapping);
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