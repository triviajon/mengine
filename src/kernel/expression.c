#include "src/kernel/expression.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/linear_map.h"
#include "src/common/map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/definitional_equal.h"
#include "src/kernel/inductive.h"
#include "src/kernel/order.h"
#include "src/kernel/structural.h"
#include "src/kernel/subst.h"

// Intrusive singly-linked list of all allocated Expressions for deferred GC shutdown.
static Expression *g_expr_list_head = NULL;

// Register a body to a recursive variable expression. WARNING: Mutator function, modifies the
// expression in place and returns true if successful, false otherwise.
bool register_fix_body_to_expression(Context *recursive_var, Expression *body) {
    if (recursive_var == NULL || body == NULL) {
        return false;
    }

    if (recursive_var->tag != VAR_EXPRESSION) {
        fprintf(stderr, ERROR "Expression is not a variable expression.\n" CRESET);
        return false;
    }

    if (recursive_var->as.var.body != NULL) {
        fprintf(stderr, ERROR "Variable already has a body.\n" CRESET);
        return false;
    }

    if (!valid_in_context(body, recursive_var)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return false;
    }

    Expression *body_type = get_expression_type(body);
    Expression *expression_type = get_expression_type(recursive_var);
    if (!definitional_equal(body_type, expression_type)) {
        fprintf(stderr, ERROR "Body type is not congruent to expression type.\n" CRESET);
        return false;
    }

    SET_VAR_BODY(recursive_var, body);

    return true;
}

DLLNode *add_to_parents(Expression *expression, void *ptr, Relation r) {
    if (expression->uplinks == NULL) {
        expression->uplinks = dll_create();
    }
    Uplink *uplink = new_uplink(ptr, r);
    DLLNode *node = dll_new_node(uplink);
    dll_insert_at_head(expression->uplinks, node);
    expression->uplink_count++;
    return node;
}

void remove_uplink_by_node(Expression *expr, DLLNode *dll_node) {
    if (expr == NULL || dll_node == NULL || expr->uplinks == NULL) {
        return;
    }
    dll_remove_node(expr->uplinks, dll_node);
    free(dll_node->data); /* free Uplink */
    free(dll_node);       /* free DLLNode */
    expr->uplink_count--;
}

void propagate_evar_refs(Expression *parent, Expression *child) {
    if (child->has_evar) {
        parent->has_evar = true;
    }
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

void remove_uplink(Expression *expr, void *parent_ptr, Relation rel) {
    if (expr == NULL || expr->uplinks == NULL) {
        return;
    }

    DLLNode *current = expr->uplinks->head;
    DLLNode *prev = NULL;

    while (current != NULL) {
        Uplink *uplink = (Uplink *)current->data;
        if (uplink->ptr == parent_ptr && uplink->relation == rel) {
            if (prev == NULL) {
                expr->uplinks->head = current->next;
                if (current->next != NULL) {
                    current->next->prev = NULL;
                } else {
                    expr->uplinks->tail = NULL;
                }
            } else {
                prev->next = current->next;
                if (current->next != NULL) {
                    current->next->prev = prev;
                } else {
                    expr->uplinks->tail = prev;
                }
            }

            free(uplink);
            free(current);
            expr->uplink_count--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Garbage Collection

void free_expression(Expression *expr) { (void)expr; }

void free_expressions_excluding_context(Expression *a, Expression *b, Expression *shared_ctx) {
    (void)a;
    (void)b;
    (void)shared_ctx;
}

void free_expression_graph(Expression *root) { (void)root; }

void free_filled_hole(Expression *hole) { (void)hole; }

// Flat free of a single expression's non-expression heap allocations.
static void gc_free_node(Expression *expr) {
    if (expr->uplinks) {
        DLLNode *node = expr->uplinks->head;
        while (node) {
            DLLNode *next = node->next;
            free(node->data);  // Uplink*
            free(node);
            node = next;
        }
        free(expr->uplinks);
    }

    // Free tag-specific non-expression allocations
    switch (expr->tag) {
        case VAR_EXPRESSION:
            order_on_delete(expr);
            free(expr->as.var.name);
            break;
        case MATCH_EXPRESSION:
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                MatchBranch *branch = expr->as.match.branches[i];
                free(branch->pattern_variables);
                free(branch);
            }
            free(expr->as.match.branches);
            break;
        case FIX_EXPRESSION:
            free(expr->as.fix.args);
            break;
        case HOLE_EXPRESSION:
            free(expr->as.hole.name);
            break;
        default:
            break;
    }

    free(expr);
}

// Walk the intrusive GC list and flat-free every tracked expression.
void expression_gc_shutdown(void) {
    Expression *expr = g_expr_list_head;
    while (expr) {
        Expression *next = expr->g_alloc_next;
        gc_free_node(expr);
        expr = next;
    }
    g_expr_list_head = NULL;
    TYPE = NULL;
    PROP = NULL;
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

    Expression *result = NULL;
    if (actual_arg_type == expected_arg_type || subtypes(actual_arg_type, expected_arg_type)) {
        result = new_subst(context, return_type, variable, arg);  // B[x -> arg]
    } else {
        fprintf(stderr, ERROR "Application does not type check.\n" CRESET);
    }
    return result;
}

Expression *_init_expression_base(ExpressionType tag, Context *context, int ctx_size,
                                  Expression *type) {
    Expression *expr = (Expression *)calloc(1, sizeof(Expression));
    if (!expr) {
        return NULL;
    }

    // Track in global GC list
    expr->g_alloc_next = g_expr_list_head;
    g_expr_list_head = expr;

    SET_EXPR_TAG(expr, tag);
    // uplinks are lazily allocated in add_to_parents
    SET_EXPR_CONTEXT(expr, context);
    SET_EXPR_CTX_SIZE(expr, ctx_size);
    SET_EXPR_TYPE(expr, type);

    return expr;
}

Expression *init_prop_expression() {
    if (PROP == NULL) {
        PROP =
            _init_expression_base(/* tag */ PROP_EXPRESSION, /* context */ context_create_empty(),
                                  /* ctx_size */ 0, /* type */ init_type_expression());
    }
    return PROP;
}

Expression *init_type_expression() {
    if (TYPE == NULL) {
        Context *context = context_create_empty();
        // Special case: Type's type is recursive, so we need to create it manually.
        TYPE = calloc(1, sizeof(Expression));
        if (!TYPE) {
            return NULL;
        }
        TYPE->g_alloc_next = g_expr_list_head;
        g_expr_list_head = TYPE;

        SET_EXPR_TAG(TYPE, TYPE_EXPRESSION);
        SET_EXPR_CONTEXT(TYPE, context);
        SET_EXPR_CTX_SIZE(TYPE, 0);
        SET_EXPR_TYPE(TYPE, init_type_expression());
    }
    return TYPE;
}

Expression *init_hole_expression(char *name, Expression *type, Context *gamma) {
    if (!valid_in_context(type, gamma)) {
        fprintf(stderr, ERROR "Type is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *type_type = get_expression_type(type);
    if (type_type->tag != PROP_EXPRESSION && type_type->tag != TYPE_EXPRESSION) {
        fprintf(stderr, ERROR "Type is not a Prop or Type_i.\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(/* tag */ HOLE_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ type);

    SET_HOLE_NAME(expr, strdup(name));
    expr->has_evar = true;  // A hole always contains itself
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
                                             /* type */ type);

    SET_VAR_NAME(expr, strdup(name));
    order_on_insert(gamma, expr);
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
                                             /* type */ type);

    SET_VAR_NAME(expr, strdup(name));
    SET_VAR_BODY(expr, body);
    propagate_evar_refs(expr, body);
    order_on_insert(gamma, expr);
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
                              /* type */ _construct_lambda_type(bound_variable, body));

    SET_LAMBDA_BOUND_VAR(expr, bound_variable);
    SET_LAMBDA_BODY(expr, body);
    propagate_evar_refs(expr, body);

    return expr;
}

Expression *init_app_expression_wc(Expression *func, Expression *arg, Context *context) {
    if (!func || !arg || !context) {
        fprintf(stderr, ERROR "Application input is NULL.\n" CRESET);
        return NULL;
    }

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

    Expression *type = _construct_app_type(context, func, arg);
    if (!type) {
        return NULL;
    }

    Expression *expr = _init_expression_base(
        /* tag */ APP_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ type);

    SET_APP_FUNC(expr, func);
    SET_APP_ARG(expr, arg);
    propagate_evar_refs(expr, func);
    propagate_evar_refs(expr, arg);

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
                                             /* type */ body_type);

    SET_FORALL_BOUND_VAR(expr, bound_variable);
    SET_FORALL_BODY(expr, body);
    propagate_evar_refs(expr, body);

    return expr;
}

Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs, Context *gamma) {
    // lhs -> rhs <-> Forall _: lhs, rhs
    Expression *unnamed_variable = init_var_expression_wc("_", lhs, gamma);
    return init_forall_expression_wc(unnamed_variable, rhs);
}

Expression *init_match_expression_wc(Expression *scrutinee, MatchBranch **branches,
                                     int branch_count, Context *context) {
    // gamma |- scrutinee : I
    if (!valid_in_context(scrutinee, context)) {
        fprintf(stderr, ERROR "Match scrutinee is not valid in context.\n" CRESET);
        return NULL;
    }

    Expression *scrutinee_type = get_expression_type(scrutinee);
    if (!is_inductive(scrutinee_type)) {
        fprintf(stderr, ERROR "Match scrutinee must have an inductive type.\n" CRESET);
        return NULL;
    }

    // For each branch i: gamma, pattern_vars[i] : type(pattern_vars[i]) |- body_i : T
    int expected_ctor_count;
    Expression **expected_ctors = get_constructors(scrutinee_type, &expected_ctor_count);
    if (!expected_ctors || expected_ctor_count == 0) {
        fprintf(stderr, ERROR "Failed to get constructors for inductive type.\n" CRESET);
        return NULL;
    }

    if (branch_count != expected_ctor_count) {
        fprintf(stderr, ERROR "Non-exhaustive match: expected %d branches, got %d.\n" CRESET,
                expected_ctor_count, branch_count);
        return NULL;
    }

    // We combine two check here: 1) use a bool array to track which constructors have been covered,
    // and 2) verify gamma, pattern_vars[i] : type(pattern_vars[i]) |- body_i : T
    bool *constructor_covered = calloc(expected_ctor_count, sizeof(bool));
    Expression *match_type = NULL;

    for (int i = 0; i < branch_count; i++) {
        MatchBranch *branch = branches[i];

        // Find which constructor this branch matches
        int ctor_idx = -1;
        for (int j = 0; j < expected_ctor_count; j++) {
            if (congruence(branch->constructor, expected_ctors[j])) {
                ctor_idx = j;
                break;
            }
        }

        if (ctor_idx == -1) {
            fprintf(stderr, ERROR "Branch constructor not found in inductive type.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }

        if (constructor_covered[ctor_idx]) {
            fprintf(stderr, ERROR "Duplicate pattern for constructor.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }
        constructor_covered[ctor_idx] = true;

        // Verify gamma, pattern_vars[i] : type(pattern_vars[i]) |- body[i] : T
        // TODO: rethink how match expression contexts should be handled
        Expression *ctor_type = get_expression_type(branch->constructor);
        int expected_args = 0;
        Expression *temp_type = ctor_type;
        while (temp_type->tag == FORALL_EXPRESSION) {
            expected_args++;
            temp_type = get_forall_body(temp_type);
        }

        if (branch->pattern_var_count != expected_args) {
            fprintf(stderr, ERROR "Pattern variable count mismatch: expected %d, got %d.\n" CRESET,
                    expected_args, branch->pattern_var_count);
            free(constructor_covered);
            return NULL;
        }

        Expression *branch_body_bound = branch->body;
        if (branch->pattern_var_count == 0 && !valid_in_context(branch_body_bound, context)) {
            fprintf(stderr, ERROR "Branch body is not valid in context.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }

        // All branches must have the same type up to definitional equality.
        Expression *branch_body_type = get_expression_type(branch->body);
        if (match_type == NULL) {
            match_type = branch_body_type;
        } else if (!definitional_equal(branch_body_type, match_type)) {
            fprintf(stderr, ERROR "Branch body types do not match.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }
    }

    free(constructor_covered);

    if (!match_type) {
        fprintf(stderr, ERROR "Match expression has no type (no branches).\n" CRESET);
        return NULL;
    }

    Expression *expr = _init_expression_base(
        /* tag */ MATCH_EXPRESSION, /* context */ context,
        /* ctx_size */ context->ctx_size,
        /* type */ match_type);

    expr->as.match.branch_count = branch_count;  // Set branch_count BEFORE calling macros
    SET_MATCH_SCRUTINEE(expr, scrutinee);
    SET_MATCH_BRANCHES(expr, branches);
    propagate_evar_refs(expr, scrutinee);
    for (int i = 0; i < branch_count; i++) {
        propagate_evar_refs(expr, branches[i]->body);
    }

    return expr;
}

// Helper to extract the nth argument from an application chain
Expression *get_nth_app_arg(Expression *app, int n) {
    DoublyLinkedList *args = dll_create();
    Expression *curr = app;

    while (curr->tag == APP_EXPRESSION) {
        dll_insert_at_head(args, dll_new_node(get_app_arg(curr)));
        curr = get_app_func(curr);
    }

    if (n >= dll_len(args)) {
        dll_destroy(args);
        return NULL;
    }

    Expression *result = dll_at(args, n)->data;
    dll_destroy(args);
    return result;
}

// Map from pattern variables to their scrutinee
// Records: "pattern_var is directly structurally smaller than scrutinee"
static bool check_all_recursive_calls_with_context(Expression *body, Expression *rec_var,
                                                   Expression *decreasing_arg, int decreasing_idx,
                                                   LinearMap *pattern_to_scrutinee);

static bool check_all_recursive_calls(Expression *body, Expression *rec_var,
                                      Expression *decreasing_arg, int decreasing_idx) {
    LinearMap *empty_map = linear_map_new();
    bool result = check_all_recursive_calls_with_context(body, rec_var, decreasing_arg,
                                                         decreasing_idx, empty_map);
    linear_map_free(empty_map);
    return result;
}

static bool check_all_recursive_calls_with_context(Expression *body, Expression *rec_var,
                                                   Expression *decreasing_arg, int decreasing_idx,
                                                   LinearMap *pattern_to_scrutinee) {
    switch (body->tag) {
        case VAR_EXPRESSION:
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
        case HOLE_EXPRESSION:
            return true;

        case APP_EXPRESSION: {
            Expression *head = get_head(body);
            if (congruence(head, rec_var)) {
                Expression *actual_arg = get_nth_app_arg(body, decreasing_idx);

                // Check if actual_arg is a pattern variable
                Expression *scrutinee_of_pattern = linear_map_get(pattern_to_scrutinee, actual_arg);
                if (scrutinee_of_pattern) {
                    // actual_arg is directly structurally smaller than scrutinee_of_pattern
                    // Check if scrutinee_of_pattern is the decreasing_arg or smaller
                    if (congruence(scrutinee_of_pattern, decreasing_arg)) {
                        return true;
                    }
                    return term_structurally_smaller_than_arg(scrutinee_of_pattern, decreasing_arg);
                }

                return term_structurally_smaller_than_arg(actual_arg, decreasing_arg);
            }

            // Recursively check subexpressions
            return (check_all_recursive_calls_with_context(get_app_func(body), rec_var,
                                                           decreasing_arg, decreasing_idx,
                                                           pattern_to_scrutinee) &&
                    check_all_recursive_calls_with_context(get_app_arg(body), rec_var,
                                                           decreasing_arg, decreasing_idx,
                                                           pattern_to_scrutinee)) != 0;
        }

        case LAMBDA_EXPRESSION:
            return check_all_recursive_calls_with_context(get_lambda_body(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee);

        case FORALL_EXPRESSION:
            return check_all_recursive_calls_with_context(get_forall_body(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee);

        case MATCH_EXPRESSION: {
            if (!check_all_recursive_calls_with_context(body->as.match.scrutinee, rec_var,
                                                        decreasing_arg, decreasing_idx,
                                                        pattern_to_scrutinee)) {
                return false;
            }

            Expression *scrutinee = body->as.match.scrutinee;

            for (int i = 0; i < body->as.match.branch_count; i++) {
                MatchBranch *branch = body->as.match.branches[i];

                // Create extended map for this branch
                LinearMap *branch_map = linear_map_new();

                // Copy existing mappings
                for (int j = 0; j < pattern_to_scrutinee->size; j++) {
                    LinearMapItem *item = &pattern_to_scrutinee->items[j];
                    if (item->key) {
                        linear_map_set(branch_map, item->key, item->val);
                    }
                }

                // Add new mappings: each pattern variable is directly structurally smaller than
                // scrutinee
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    linear_map_set(branch_map, branch->pattern_variables[j], scrutinee);
                }

                bool branch_ok = check_all_recursive_calls_with_context(
                    branch->body, rec_var, decreasing_arg, decreasing_idx, branch_map);

                linear_map_clear_free(branch_map);

                if (!branch_ok) {
                    return false;
                }
            }
            return true;
        }

        case FIX_EXPRESSION:
            return check_all_recursive_calls_with_context(
                body->as.fix.body, rec_var, decreasing_arg, decreasing_idx, pattern_to_scrutinee);

        default:
            return true;
    }
}

Expression *init_fix_expression_wc(Expression *recursive_var, Expression **args, int arg_count,
                                   int decreasing_arg_index, Expression *body) {
    Context *gamma = get_expression_context(recursive_var);
    int expected_args = 0;
    Expression *temp_type = get_expression_type(recursive_var);
    while (temp_type->tag == FORALL_EXPRESSION) {
        expected_args++;
        temp_type = get_forall_body(temp_type);
    }

    if (arg_count != expected_args) {
        fprintf(stderr, ERROR "Argument count mismatch: expected %d, got %d.\n" CRESET,
                expected_args, arg_count);
        return NULL;
    }

    // Build extended context: gamma -> recursive_var -> args[0] -> ... -> args[n-1]
    Context *extended = recursive_var;
    for (int i = 0; i < arg_count; i++) {
        extended = args[i];
    }

    if (!valid_in_context(body, extended)) {
        fprintf(stderr, ERROR "Body is not valid in context.\n" CRESET);
        return NULL;
    }

    // Check all recursive calls satisfy structural recursion
    if (decreasing_arg_index >= 0 && decreasing_arg_index < arg_count) {
        Expression *decreasing_arg = args[decreasing_arg_index];
        if (!check_all_recursive_calls(body, recursive_var, decreasing_arg, decreasing_arg_index)) {
            fprintf(stderr, ERROR "Structural recursion check failed.\n" CRESET);
            return NULL;
        }
    }

    Expression *body_bound = body;
    for (int i = arg_count - 1; i >= 0; i--) {
        body_bound = init_lambda_expression_wc(args[i], body_bound);
        if (!body_bound) {
            fprintf(stderr, ERROR "Failed to create body bound.\n" CRESET);
            return NULL;
        }
    }

    Expression *expr = _init_expression_base(/* tag */ FIX_EXPRESSION, /* context */ gamma,
                                             /* ctx_size */ gamma->ctx_size,
                                             /* type */ get_expression_type(recursive_var));

    // Copy args into a fresh heap allocation so gc_free_node can safely free it.
    Expression **args_copy = malloc(arg_count * sizeof(Expression *));
    for (int i = 0; i < arg_count; i++) {
        args_copy[i] = args[i];
    }

    SET_FIX_RECURSIVE_VAR(expr, recursive_var);
    SET_FIX_ARG_COUNT(expr, arg_count);
    SET_FIX_ARGS(expr, args_copy);
    SET_FIX_DECREASING_ARG_INDEX(expr, decreasing_arg_index);
    SET_FIX_BODY(expr, body);
    propagate_evar_refs(expr, body);

    if (!register_fix_body_to_expression(recursive_var, body_bound)) {
        fprintf(stderr, ERROR "Failed to register body to recursive variable.\n" CRESET);
        free_expression(expr);
        return NULL;
    }

    return expr;
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
        case (MATCH_EXPRESSION):
            return expression->uplinks;
        case (FIX_EXPRESSION):
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

Expression *get_head(Expression *e) {
    if (e->tag == APP_EXPRESSION) {
        return get_head(e->as.app.func);
    }
    return e;
}

char *get_var_name(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expr->as.var.name;
}

Expression *get_var_body(Expression *expr) {
    if (expr->tag != VAR_EXPRESSION) {
        return NULL;
    }

    return expr->as.var.body;
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

Expression *get_match_scrutinee(Expression *expr) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.scrutinee;
}

Expression *get_match_branch_body(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.branches[index]->body;
}

Expression **get_match_branch_pattern_variables(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return NULL;
    }

    return expr->as.match.branches[index]->pattern_variables;
}

int get_match_branch_pattern_var_count(Expression *expr, int index) {
    if (expr->tag != MATCH_EXPRESSION) {
        return -1;
    }

    return expr->as.match.branches[index]->pattern_var_count;
}

Expression *get_fix_recursive_var(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.recursive_var;
}

Expression **get_fix_args(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.args;
}

int get_fix_arg_count(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return -1;
    }

    return expr->as.fix.arg_count;
}

int get_fix_decreasing_arg_index(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return -1;
    }

    return expr->as.fix.decreasing_arg_index;
}

Expression *get_fix_body(Expression *expr) {
    if (expr->tag != FIX_EXPRESSION) {
        return NULL;
    }

    return expr->as.fix.body;
}

int get_arity(Expression *expr) {
    Expression *type = get_expression_type(expr);
    int arity = 0;

    while (type->tag == FORALL_EXPRESSION) {
        arity++;
        type = get_forall_body(type);
    }

    return arity;
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
            return (_congruence(a->as.app.func, b->as.app.func, mapping) &&
                    _congruence(a->as.app.arg, b->as.app.arg, mapping)) != 0;
        case (FORALL_EXPRESSION): {
            Expression *bv_a = a->as.forall.bound_variable;
            Expression *bv_b = b->as.forall.bound_variable;
            map_set(mapping, bv_a, bv_b);
            bool result = _congruence(a->as.forall.body, b->as.forall.body, mapping);
            map_del(mapping, bv_a);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            Expression *bv_a = a->as.lambda.bound_variable;
            Expression *bv_b = b->as.lambda.bound_variable;
            map_set(mapping, bv_a, bv_b);
            bool result = _congruence(a->as.lambda.body, b->as.lambda.body, mapping);
            map_del(mapping, bv_a);
            return result;
        }
        case (VAR_EXPRESSION): {
            return ((a == b) || (map_get(mapping, a) == b)) != 0;
        }
        case (HOLE_EXPRESSION): {
            return ((a == b) || (map_get(mapping, a) == b)) != 0;
        }
        case (MATCH_EXPRESSION): {
            if (!_congruence(a->as.match.scrutinee, b->as.match.scrutinee, mapping)) {
                return false;
            }
            if (a->as.match.branch_count != b->as.match.branch_count) {
                return false;
            }
            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *branch_a = a->as.match.branches[i];
                MatchBranch *branch_b = b->as.match.branches[i];

                if (!_congruence(branch_a->constructor, branch_b->constructor, mapping)) {
                    return false;
                }
                if (branch_a->pattern_var_count != branch_b->pattern_var_count) {
                    return false;
                }

                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    map_set(mapping, branch_a->pattern_variables[j],
                            branch_b->pattern_variables[j]);
                }

                bool body_result = _congruence(branch_a->body, branch_b->body, mapping);

                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    map_del(mapping, branch_a->pattern_variables[j]);
                }

                if (!body_result) {
                    return false;
                }
            }
            return true;
        }
        case (FIX_EXPRESSION): {
            Expression *rv_a = a->as.fix.recursive_var;
            Expression *rv_b = b->as.fix.recursive_var;
            map_set(mapping, rv_a, rv_b);

            if (a->as.fix.arg_count != b->as.fix.arg_count) {
                map_del(mapping, rv_a);
                return false;
            }

            if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                map_del(mapping, rv_a);
                return false;
            }

            for (int i = 0; i < a->as.fix.arg_count; i++) {
                map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }

            bool result = _congruence(a->as.fix.body, b->as.fix.body, mapping);

            for (int i = 0; i < a->as.fix.arg_count; i++) {
                map_del(mapping, a->as.fix.args[i]);
            }
            map_del(mapping, rv_a);
            return result;
        }
    }
    return false;
}

bool congruence(Expression *a, Expression *b) {
    Map *mapping = map_new_with_capacity(8);
    bool result = _congruence(a, b, mapping);
    map_free(mapping);
    return result;
}

bool subtypes(Expression *a, Expression *b) {
    // We don't implement a full subtyping relation, but it is necessary
    // specifically for Type and Prop.
    if (a->tag == PROP_EXPRESSION && b->tag == TYPE_EXPRESSION) {
        return true;
    }

    return definitional_equal(a, b);
}

void _match_and_subst(Expression *a, Expression *b, LinearMap *mapping) {
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
            linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            _match_and_subst(a->as.forall.body, b->as.forall.body, mapping);
            break;
        }
        case (LAMBDA_EXPRESSION): {
            linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            _match_and_subst(a->as.lambda.body, b->as.lambda.body, mapping);
            break;
        }
        case (VAR_EXPRESSION): {
            if (a != b) {
                (linear_map_set(mapping, a, b));
            }
            break;
        }
        case (HOLE_EXPRESSION): {
            if (a != b) {
                (linear_map_set(mapping, a, b));
            }
            break;
        }
        case (MATCH_EXPRESSION): {
            _match_and_subst(a->as.match.scrutinee, b->as.match.scrutinee, mapping);
            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *branch_a = a->as.match.branches[i];
                MatchBranch *branch_b = b->as.match.branches[i];
                _match_and_subst(branch_a->constructor, branch_b->constructor, mapping);
                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    linear_map_set(mapping, branch_a->pattern_variables[j],
                                   branch_b->pattern_variables[j]);
                }
                _match_and_subst(branch_a->body, branch_b->body, mapping);
            }
            break;
        }
        case (FIX_EXPRESSION): {
            linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);
            for (int i = 0; i < a->as.fix.arg_count; i++) {
                linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }
            _match_and_subst(a->as.fix.body, b->as.fix.body, mapping);
            break;
        }
        default:
            fprintf(stderr, ERROR "Unsupported expression type in _match_and_subst.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

Expression *match_and_subst(Expression *a, Expression *b, Expression *to_subst) {
    LinearMap *mapping = linear_map_new();
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

bool has_holes(Expression *expr) { return expr->has_evar; }

bool is_hole(Expression *expr) { return expr->tag == HOLE_EXPRESSION; }

// Recompute has_evar for a single expression from its current children.
// Mirrors exactly which children propagate_evar_refs propagated from.
static bool recompute_has_evar(Expression *expr) {
    switch (expr->tag) {
        case HOLE_EXPRESSION:
            return true;  // holes are always holey (even filled ones aren't reachable from parents)
        case VAR_EXPRESSION:
            return (expr->as.var.body && expr->as.var.body->has_evar) != 0;
        case LAMBDA_EXPRESSION:
            return (expr->as.lambda.body && expr->as.lambda.body->has_evar) != 0;
        case APP_EXPRESSION:
            return ((expr->as.app.func && expr->as.app.func->has_evar) ||
                    (expr->as.app.arg && expr->as.app.arg->has_evar)) != 0;
        case FORALL_EXPRESSION:
            return (expr->as.forall.body && expr->as.forall.body->has_evar) != 0;
        case MATCH_EXPRESSION: {
            if (expr->as.match.scrutinee && expr->as.match.scrutinee->has_evar) {
                return true;
            }
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                if (expr->as.match.branches[i]->body &&
                    expr->as.match.branches[i]->body->has_evar) {
                    return true;
                }
            }
            return false;
        }
        case FIX_EXPRESSION:
            return (expr->as.fix.body && expr->as.fix.body->has_evar) != 0;
        default:
            return false;
    }
}

static uint64_t g_occurs_in_gen = 0;

bool _occurs_in(Expression *var_or_hole, Expression *term, uint64_t gen) {
    if (!term) {
        return false;
    }

    size_t capacity = 64;
    size_t top = 0;
    Expression **stack = malloc(capacity * sizeof(Expression *));
    if (!stack) {
        return false;
    }

    stack[top++] = term;
    while (top > 0) {
        Expression *cur = stack[--top];
        if (!cur || cur->visit_gen == gen) {
            continue;
        }
        cur->visit_gen = gen;

        if (var_or_hole == cur) {
            free(stack);
            return true;
        }

#define OCCURS_PUSH(e)                                                                        \
    do {                                                                                      \
        Expression *_child = (e);                                                             \
        if (_child != NULL && _child->visit_gen != gen) {                                     \
            if (top == capacity) {                                                            \
                size_t new_capacity = capacity * 2;                                           \
                Expression **new_stack = realloc(stack, new_capacity * sizeof(Expression *)); \
                if (!new_stack) {                                                             \
                    free(stack);                                                              \
                    return false;                                                             \
                }                                                                             \
                stack = new_stack;                                                            \
                capacity = new_capacity;                                                      \
            }                                                                                 \
            stack[top++] = _child;                                                            \
        }                                                                                     \
    } while (0)

        switch (cur->tag) {
            case TYPE_EXPRESSION:
            case PROP_EXPRESSION:
            case VAR_EXPRESSION:
            case HOLE_EXPRESSION:
                break;

            case APP_EXPRESSION:
                OCCURS_PUSH(cur->as.app.arg);
                OCCURS_PUSH(cur->as.app.func);
                break;

            case LAMBDA_EXPRESSION:
                OCCURS_PUSH(cur->as.lambda.body);
                OCCURS_PUSH(cur->as.lambda.bound_variable);
                break;

            case FORALL_EXPRESSION:
                OCCURS_PUSH(cur->as.forall.body);
                OCCURS_PUSH(cur->as.forall.bound_variable);
                break;

            case MATCH_EXPRESSION:
                OCCURS_PUSH(cur->as.match.scrutinee);
                for (int i = 0; i < cur->as.match.branch_count; i++) {
                    MatchBranch *branch = cur->as.match.branches[i];
                    OCCURS_PUSH(branch->body);
                    for (int j = 0; j < branch->pattern_var_count; j++) {
                        OCCURS_PUSH(branch->pattern_variables[j]);
                    }
                    OCCURS_PUSH(branch->constructor);
                }
                break;

            case FIX_EXPRESSION:
                OCCURS_PUSH(cur->as.fix.body);
                for (int i = 0; i < cur->as.fix.arg_count; i++) {
                    OCCURS_PUSH(cur->as.fix.args[i]);
                }
                OCCURS_PUSH(cur->as.fix.recursive_var);
                break;

            default:
                free(stack);
                fprintf(stderr, ERROR "Unknown expression type in occurs_in.\n" CRESET);
                exit(EXIT_FAILURE);
        }
    }

    free(stack);
    return false;
}

bool occurs_in(Expression *var_or_hole, Expression *term) {
    uint64_t gen = ++g_occurs_in_gen;
    if (gen == 0) {
        // Reserve 0 as the "unvisited" stamp to avoid false hits after wraparound.
        gen = ++g_occurs_in_gen;
    }
    return _occurs_in(var_or_hole, term, gen);
}

// Create a new version of node with old_child replaced by new_child in the
// position described by rel. Removes old uplinks from node's children before
// constructing the replacement via the normal init_*_wc path.
static Expression *fill_hole_rebuild_node(Expression *node, Expression *old_child,
                                          Expression *new_child, Relation rel) {
    switch (node->tag) {
        case APP_EXPRESSION:
            if (rel == APP_FUNC) {
                remove_uplink_by_node(old_child, node->as.app.func_uplink_node);
                node->as.app.func_uplink_node = NULL;
                remove_uplink_by_node(node->as.app.arg, node->as.app.arg_uplink_node);
                node->as.app.arg_uplink_node = NULL;
                return init_app_expression_wc(new_child, node->as.app.arg, node->context);
            } else {
                remove_uplink_by_node(node->as.app.func, node->as.app.func_uplink_node);
                node->as.app.func_uplink_node = NULL;
                remove_uplink_by_node(old_child, node->as.app.arg_uplink_node);
                node->as.app.arg_uplink_node = NULL;
                return init_app_expression_wc(node->as.app.func, new_child, node->context);
            }
        case LAMBDA_EXPRESSION:
            if (rel == LAMBDA_BODY) {
                remove_uplink_by_node(node->as.lambda.bound_variable,
                                      node->as.lambda.bound_variable_uplink_node);
                node->as.lambda.bound_variable_uplink_node = NULL;
                remove_uplink_by_node(old_child, node->as.lambda.body_uplink_node);
                node->as.lambda.body_uplink_node = NULL;
                return init_lambda_expression_wc(node->as.lambda.bound_variable, new_child);
            }
            break;
        case FORALL_EXPRESSION:
            if (rel == FORALL_BODY) {
                remove_uplink_by_node(node->as.forall.bound_variable,
                                      node->as.forall.bound_variable_uplink_node);
                node->as.forall.bound_variable_uplink_node = NULL;
                remove_uplink_by_node(old_child, node->as.forall.body_uplink_node);
                node->as.forall.body_uplink_node = NULL;
                return init_forall_expression_wc(node->as.forall.bound_variable, new_child);
            }
            break;
        default:
            break;
    }
    fprintf(stderr, WARNING "fill_hole: unhandled rebuild for tag %d relation %d\n" CRESET,
            node->tag, rel);
    return node;
}

// Walk upward from start through non-EXPR_TYPE uplinks until a VAR_BODY uplink
// is found. Returns the node that is the direct body of the VAR (the structural
// root), and sets *holder_out to the VAR. Returns NULL if no VAR_BODY path.
static Expression *find_var_body_root(Expression *start, Expression **holder_out) {
    *holder_out = NULL;
    Expression *current = start;
    for (;;) {
        if (!current->uplinks || !current->uplinks->head) return NULL;
        DLLNode *ul = current->uplinks->head;
        Expression *structural_parent = NULL;
        while (ul) {
            Uplink *up = (Uplink *)ul->data;
            if (up->relation == VAR_BODY) {
                *holder_out = (Expression *)up->ptr;
                return current;
            }
            if (up->relation != EXPR_TYPE && structural_parent == NULL) {
                structural_parent = (Expression *)up->ptr;
            }
            ul = ul->next;
        }
        if (!structural_parent) return NULL;
        current = structural_parent;
    }
}

bool fill_hole(Expression *hole, Expression *term) {
    if (hole->tag != HOLE_EXPRESSION) {
        return false;
    }

    // Check preconditions:
    //   1) type(term) == expected return type of hole
    //   2) term is valid in hole's context
    //   3) term does not contain the hole (occurs check, only if term has holes)
    LinearMap *hole_assignments = linear_map_new();
    if (!open_types_compatible_collecting(get_expression_type(hole), get_expression_type(term),
                                          hole_assignments)) {
        linear_map_clear_free(hole_assignments);
        return false;
    }
    if (!valid_in_context(term, get_expression_context(hole))) {
        linear_map_clear_free(hole_assignments);
        return false;
    }
    if (term->has_evar && occurs_in(hole, term)) {
        linear_map_clear_free(hole_assignments);
        return false;
    }

    // Cascade: fill sub-holes discovered during the open_types_compatible_collecting check
    for (int i = 0; i < hole_assignments->size; i++) {
        LinearMapItem *item = &hole_assignments->items[i];
        if (item->key) {
            fill_hole((Expression *)item->key, (Expression *)item->val);
        }
    }
    linear_map_clear_free(hole_assignments);

    // This is the only place that still feels a bit hacky.
    if (hole->uplinks) {
        DLLNode *ul = hole->uplinks->head;
        while (ul) {
            DLLNode *next = ul->next;
            Uplink *up = (Uplink *)ul->data;
            if (up->relation == EXPR_TYPE) {
                Expression *ptr = (Expression *)up->ptr;
                ptr->type = term;
                remove_uplink_by_node(hole, ul);
                add_to_parents(term, ptr, EXPR_TYPE);
            }
            ul = next;
        }
    }

    // Replace hole with term in the structural proof term via bottom-up path-copying.
    // Walk the uplink chain from hole to the VAR holder, collecting each node on
    // the path. Then rebuild bottom-up: each node is replaced with a fresh copy
    // that has the updated child, reusing the original bound variables (so their
    // contexts are preserved without freshening).
    Expression *var_holder = NULL;
    Expression *struct_root = find_var_body_root(hole, &var_holder);
    if (var_holder) {
        int cap = 16;
        Expression **chain = malloc(cap * sizeof(Expression *));
        Relation *rels = malloc(cap * sizeof(Relation));
        int len = 1;
        chain[0] = hole;

        Expression *current = hole;
        while (current != struct_root) {
            DLLNode *ul = current->uplinks ? current->uplinks->head : NULL;
            Expression *parent = NULL;
            Relation parent_rel = LAMBDA_BODY;
            while (ul) {
                Uplink *up = (Uplink *)ul->data;
                if (up->relation != EXPR_TYPE && up->relation != VAR_BODY) {
                    parent = (Expression *)up->ptr;
                    parent_rel = up->relation;
                    break;
                }
                ul = ul->next;
            }
            if (!parent) break;

            if (len >= cap) {
                cap *= 2;
                chain = realloc(chain, cap * sizeof(Expression *));
                rels = realloc(rels, cap * sizeof(Relation));
            }
            rels[len - 1] = parent_rel;
            chain[len++] = parent;
            current = parent;
        }

        Expression *new_child = term;
        for (int i = 1; i < len; i++) {
            Expression *rebuilt =
                fill_hole_rebuild_node(chain[i], chain[i - 1], new_child, rels[i - 1]);
            new_child = rebuilt;
        }

        free(chain);
        free(rels);

        remove_uplink(struct_root, var_holder, VAR_BODY);
        var_holder->as.var.body = new_child;
        add_to_parents(new_child, var_holder, VAR_BODY);
    }

    // Recompute has_evar upward from the VAR holder and from any expression
    // whose type was updated above (now reachable via term's EXPR_TYPE uplinks).
    {
        DoublyLinkedList *queue = dll_create();
        if (var_holder) {
            dll_insert_at_tail(queue, dll_new_node(var_holder));
        }
        if (term->uplinks) {
            DLLNode *ul = term->uplinks->head;
            while (ul) {
                Uplink *up = (Uplink *)ul->data;
                if (up->relation == EXPR_TYPE) {
                    dll_insert_at_tail(queue, dll_new_node((Expression *)up->ptr));
                }
                ul = ul->next;
            }
        }
        while (queue->head) {
            DLLNode *n = dll_remove_head(queue);
            Expression *p = (Expression *)n->data;
            free(n);
            bool old_val = p->has_evar;
            bool new_val = recompute_has_evar(p);
            p->has_evar = new_val;
            if (new_val != old_val && p->uplinks) {
                DLLNode *pul = p->uplinks->head;
                while (pul) {
                    Expression *pp = (Expression *)((Uplink *)pul->data)->ptr;
                    dll_insert_at_tail(queue, dll_new_node(pp));
                    pul = pul->next;
                }
            }
        }
        dll_destroy(queue);
    }

    hole->as.hole.is_satisfied = true;
    return true;
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

bool _congruence2(Expression *a, Expression *b, LinearMap *mapping) {
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
                return (_congruence2(a->as.app.func, b->as.app.func, mapping) &&
                        _congruence2(a->as.app.arg, b->as.app.arg, mapping)) != 0;
            case (FORALL_EXPRESSION): {
                linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
                return _congruence2(a->as.forall.body, b->as.forall.body, mapping);
            }
            case (LAMBDA_EXPRESSION): {
                linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
                return _congruence2(a->as.lambda.body, b->as.lambda.body, mapping);
            }
            case (VAR_EXPRESSION): {
                return ((a == b) || (linear_map_get(mapping, a) == b)) != 0;
            }
            case (HOLE_EXPRESSION): {
                return ((a == b) || (linear_map_get(mapping, a) == b)) != 0;
            }
            case (MATCH_EXPRESSION): {
                if (!_congruence2(a->as.match.scrutinee, b->as.match.scrutinee, mapping)) {
                    return false;
                }
                if (a->as.match.branch_count != b->as.match.branch_count) {
                    return false;
                }
                for (int i = 0; i < a->as.match.branch_count; i++) {
                    MatchBranch *branch_a = a->as.match.branches[i];
                    MatchBranch *branch_b = b->as.match.branches[i];
                    if (!_congruence2(branch_a->constructor, branch_b->constructor, mapping)) {
                        return false;
                    }
                    if (branch_a->pattern_var_count != branch_b->pattern_var_count) {
                        return false;
                    }
                    for (int j = 0; j < branch_a->pattern_var_count; j++) {
                        linear_map_set(mapping, branch_a->pattern_variables[j],
                                       branch_b->pattern_variables[j]);
                    }
                    if (!_congruence2(branch_a->body, branch_b->body, mapping)) {
                        return false;
                    }
                }
                return true;
            }
            case (FIX_EXPRESSION): {
                linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);
                if (a->as.fix.arg_count != b->as.fix.arg_count) {
                    return false;
                }
                if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                    return false;
                }
                for (int i = 0; i < a->as.fix.arg_count; i++) {
                    linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
                }
                return _congruence2(a->as.fix.body, b->as.fix.body, mapping);
            }
            default:
                fprintf(stderr, ERROR "Unknown expression type in _congruence2.\n" CRESET);
                return false;
        }
    } else {
        if (a->tag == HOLE_EXPRESSION || b->tag == HOLE_EXPRESSION) {
            linear_map_set(mapping, a, b);
            return true;
        }
    }

    return false;
}

bool congruence2(Expression *a, Expression *b) {
    LinearMap *mapping = linear_map_new();
    bool result = _congruence2(a, b, mapping);
    free(mapping->items);
    free(mapping);
    return result;
}