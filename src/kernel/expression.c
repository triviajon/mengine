#include "src/kernel/expression.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/linear_map.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/definitional_equal.h"
#include "src/kernel/inductive.h"
#include "src/kernel/structural.h"
#include "src/kernel/subst.h"

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
    if (!(congruence(body_type, expression_type))) {
        fprintf(stderr, ERROR "Body type is not congruent to expression type.\n" CRESET);
        return false;
    }

    SET_VAR_BODY(recursive_var, body);

    return true;
}

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
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Forward declaration
static void free_expression_field(Expression *parent, Expression *child, Relation rel);

void free_expression(Expression *expr) {
    // Never free globals
    if (expr == NULL || expr == TYPE || expr == PROP || expr == EMPTY_CONTEXT) {
        return;
    }

    // Remove this expression from all its children's uplink lists
    free_expression_field(expr, expr->type, EXPR_TYPE);
    free_expression_field(expr, expr->context, EXPR_CONTEXT);

    // Handle expression-specific fields
    switch (expr->tag) {
        case VAR_EXPRESSION:
            if (expr->as.var.body != NULL) {
                free_expression_field(expr, expr->as.var.body, VAR_BODY);
            }
            free(expr->as.var.name);
            break;
        case LAMBDA_EXPRESSION:
            free_expression_field(expr, expr->as.lambda.bound_variable, LAMBDA_BOUND_VAR);
            free_expression_field(expr, expr->as.lambda.body, LAMBDA_BODY);
            break;
        case APP_EXPRESSION:
            free_expression_field(expr, expr->as.app.func, APP_FUNC);
            free_expression_field(expr, expr->as.app.arg, APP_ARG);
            break;
        case FORALL_EXPRESSION:
            free_expression_field(expr, expr->as.forall.bound_variable, FORALL_BOUND_VAR);
            free_expression_field(expr, expr->as.forall.body, FORALL_BODY);
            break;
        case MATCH_EXPRESSION:
            free_expression_field(expr, expr->as.match.scrutinee, MATCH_SCRUTINEE);
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                MatchBranch *branch = expr->as.match.branches[i];
                free_expression_field(expr, branch->constructor, MATCH_BRANCH_CONSTRUCTOR);
                free_expression_field(expr, branch->body, MATCH_BRANCH_BODY);
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    free_expression_field(expr, branch->pattern_variables[j],
                                          MATCH_BRANCH_PATTERN_VAR);
                }
                free(branch->pattern_variables);
                free(branch);
            }
            free(expr->as.match.branches);
            break;
        case FIX_EXPRESSION:
            free_expression_field(expr, expr->as.fix.recursive_var, FIX_RECURSIVE_VAR);
            for (int i = 0; i < expr->as.fix.arg_count; i++) {
                free_expression_field(expr, expr->as.fix.args[i], FIX_ARG);
            }
            free(expr->as.fix.args);
            free_expression_field(expr, expr->as.fix.body, FIX_BODY);
            break;
        case HOLE_EXPRESSION:
            free(expr->as.hole.name);
            break;
        case TYPE_EXPRESSION:
        case PROP_EXPRESSION:
            break;
    }

    dll_destroy(expr->uplinks);
    free(expr);
}

static void free_expression_field(Expression *parent, Expression *child, Relation rel) {
    if (child == NULL) {
        return;
    }

    // Remove parent from child's uplinks
    remove_uplink(child, parent, rel);

    // Check if child is now unreachable (no more uplinks)
    if (dll_len(child->uplinks) == 0) {
        free_expression(child);
    }
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
    Expression *expr = (Expression *)calloc(1, sizeof(Expression));
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
                                             /* type */ type,
                                             /* maybe_hole_free */ false);

    SET_HOLE_NAME(expr, strdup(name));
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
        for (int j = branch->pattern_var_count - 1; j >= 0; j--) {
            branch_body_bound =
                init_lambda_expression_wc(branch->pattern_variables[j], branch_body_bound);
            if (!branch_body_bound) {
                fprintf(stderr, ERROR "Failed to create branch body bound.\n" CRESET);
                free(constructor_covered);
                return NULL;
            }
        }

        if (!valid_in_context(branch_body_bound, context)) {
            fprintf(stderr, ERROR "Branch body is not valid in context.\n" CRESET);
            free(constructor_covered);
            return NULL;
        }

        // All branches must have the same type
        Expression *branch_body_type = get_expression_type(branch->body);
        if (match_type == NULL) {
            match_type = branch_body_type;
        } else if (!congruence(branch_body_type, match_type)) {
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
        /* type */ match_type,
        /* maybe_hole_free */ get_maybe_hole_free(scrutinee));

    expr->as.match.branch_count = branch_count;  // Set branch_count BEFORE calling macros
    SET_MATCH_SCRUTINEE(expr, scrutinee);
    SET_MATCH_BRANCHES(expr, branches);

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
            return check_all_recursive_calls_with_context(get_app_func(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee) &&
                   check_all_recursive_calls_with_context(get_app_arg(body), rec_var,
                                                          decreasing_arg, decreasing_idx,
                                                          pattern_to_scrutinee);
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
                                             /* type */ get_expression_type(recursive_var),
                                             /* maybe_hole_free */ get_maybe_hole_free(body));

    SET_FIX_RECURSIVE_VAR(expr, recursive_var);
    SET_FIX_ARGS(expr, args);
    SET_FIX_ARG_COUNT(expr, arg_count);
    SET_FIX_DECREASING_ARG_INDEX(expr, decreasing_arg_index);
    SET_FIX_BODY(expr, body);

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
bool _congruence(Expression *a, Expression *b, LinearMap *mapping) {
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
            linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
            return _congruence(a->as.forall.body, b->as.forall.body, mapping);
        }
        case (LAMBDA_EXPRESSION): {
            linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
            return _congruence(a->as.lambda.body, b->as.lambda.body, mapping);
        }
        case (VAR_EXPRESSION): {
            return (a == b) || (linear_map_get(mapping, a) == b);
        }
        case (HOLE_EXPRESSION): {
            return (a == b) || (linear_map_get(mapping, a) == b);
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

                // Map pattern variables
                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    linear_map_set(mapping, branch_a->pattern_variables[j],
                                   branch_b->pattern_variables[j]);
                }

                if (!_congruence(branch_a->body, branch_b->body, mapping)) {
                    return false;
                }
            }
            return true;
        }
        case (FIX_EXPRESSION): {
            // Map recursive variable
            linear_map_set(mapping, a->as.fix.recursive_var, b->as.fix.recursive_var);

            // Check arg counts match
            if (a->as.fix.arg_count != b->as.fix.arg_count) {
                return false;
            }

            // Check decreasing arg index matches
            if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                return false;
            }

            // Map all args and check congruence
            for (int i = 0; i < a->as.fix.arg_count; i++) {
                linear_map_set(mapping, a->as.fix.args[i], b->as.fix.args[i]);
            }

            // Check body congruence
            return _congruence(a->as.fix.body, b->as.fix.body, mapping);
        }
    }
}

bool congruence(Expression *a, Expression *b) {
    LinearMap *mapping = linear_map_new();
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

bool _congruent_with_holes(Expression *a, Expression *b, LinearMap *alpha_equivalences,
                           LinearMap *required_holes) {
    if (a == b) {
        return true;
    }

    if (a->tag == HOLE_EXPRESSION && b->tag == HOLE_EXPRESSION) {
        return false;  // TODO: what do we do in this case?
    }
    if (a->tag == HOLE_EXPRESSION) {
        bool b_can_fill = can_fill(a, b);
        if (b_can_fill) {
            linear_map_set(required_holes, a, b);
            return true;
        }
        return false;
    }
    if (b->tag == HOLE_EXPRESSION) {
        bool a_can_fill = can_fill(b, a);
        if (a_can_fill) {
            linear_map_set(required_holes, b, a);
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
            linear_map_set(alpha_equivalences, a->as.forall.bound_variable,
                           b->as.forall.bound_variable);
            bool result = _congruent_with_holes(a->as.forall.body, b->as.forall.body,
                                                alpha_equivalences, required_holes);
            return result;
        }
        case (LAMBDA_EXPRESSION): {
            linear_map_set(alpha_equivalences, a->as.lambda.bound_variable,
                           b->as.lambda.bound_variable);
            bool result = _congruent_with_holes(a->as.lambda.body, b->as.lambda.body,
                                                alpha_equivalences, required_holes);
            return result;
        }
        case (VAR_EXPRESSION): {
            return (a == b) || linear_map_get(alpha_equivalences, a) == b;
        }
        case (MATCH_EXPRESSION): {
            if (!_congruent_with_holes(a->as.match.scrutinee, b->as.match.scrutinee,
                                       alpha_equivalences, required_holes)) {
                return false;
            }
            if (a->as.match.branch_count != b->as.match.branch_count) {
                return false;
            }
            for (int i = 0; i < a->as.match.branch_count; i++) {
                MatchBranch *branch_a = a->as.match.branches[i];
                MatchBranch *branch_b = b->as.match.branches[i];
                if (!_congruent_with_holes(branch_a->constructor, branch_b->constructor,
                                           alpha_equivalences, required_holes)) {
                    return false;
                }
                if (branch_a->pattern_var_count != branch_b->pattern_var_count) {
                    return false;
                }
                for (int j = 0; j < branch_a->pattern_var_count; j++) {
                    linear_map_set(alpha_equivalences, branch_a->pattern_variables[j],
                                   branch_b->pattern_variables[j]);
                }
                if (!_congruent_with_holes(branch_a->body, branch_b->body, alpha_equivalences,
                                           required_holes)) {
                    return false;
                }
            }
            return true;
        }
        case (FIX_EXPRESSION): {
            linear_map_set(alpha_equivalences, a->as.fix.recursive_var, b->as.fix.recursive_var);
            if (a->as.fix.arg_count != b->as.fix.arg_count) {
                return false;
            }
            if (a->as.fix.decreasing_arg_index != b->as.fix.decreasing_arg_index) {
                return false;
            }
            for (int i = 0; i < a->as.fix.arg_count; i++) {
                linear_map_set(alpha_equivalences, a->as.fix.args[i], b->as.fix.args[i]);
            }
            return _congruent_with_holes(a->as.fix.body, b->as.fix.body, alpha_equivalences,
                                         required_holes);
        }
        default:
            fprintf(stderr, ERROR "Unknown expression type in _congruent_with_holes.\n" CRESET);
            return false;
    }
}

bool congruent_with_holes(Expression *a, Expression *b) {
    LinearMap *alpha_equivalences = linear_map_new();
    LinearMap *required_holes = linear_map_new();
    bool result = _congruent_with_holes(a, b, alpha_equivalences, required_holes);
    linear_map_clear_free(alpha_equivalences);
    linear_map_clear_free(required_holes);
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
        case (MATCH_EXPRESSION): {
            if (has_holes(expr->as.match.scrutinee)) {
                return true;
            }
            for (int i = 0; i < expr->as.match.branch_count; i++) {
                if (has_holes(expr->as.match.branches[i]->body)) {
                    return true;
                }
            }
            return false;
        }
        case (FIX_EXPRESSION):
            return has_holes(expr->as.fix.body);
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
    bool types_match = definitional_equal(get_expression_type(hole), get_expression_type(term));
    if (get_maybe_hole_free(term)) {
        return types_match && valid_in_context(term, get_expression_context(hole));
    }
    bool occurs = occurs_in(hole, term);
    return types_match && valid_in_context(term, get_expression_context(hole)) && !occurs;
}

bool _occurs_in(Expression *var_or_hole, Expression *term, LinearMap *visited) {
    if (linear_map_get(visited, term) != NULL) {
        return false;
    }
    linear_map_set(visited, term, term);

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
        case MATCH_EXPRESSION: {
            if (_occurs_in(var_or_hole, term->as.match.scrutinee, visited)) {
                return true;
            }
            for (int i = 0; i < term->as.match.branch_count; i++) {
                MatchBranch *branch = term->as.match.branches[i];
                if (_occurs_in(var_or_hole, branch->constructor, visited)) {
                    return true;
                }
                for (int j = 0; j < branch->pattern_var_count; j++) {
                    if (_occurs_in(var_or_hole, branch->pattern_variables[j], visited)) {
                        return true;
                    }
                }
                if (_occurs_in(var_or_hole, branch->body, visited)) {
                    return true;
                }
            }
            return false;
        }
        case FIX_EXPRESSION:
            if (_occurs_in(var_or_hole, term->as.fix.recursive_var, visited)) {
                return true;
            }
            for (int i = 0; i < term->as.fix.arg_count; i++) {
                if (_occurs_in(var_or_hole, term->as.fix.args[i], visited)) {
                    return true;
                }
            }
            return _occurs_in(var_or_hole, term->as.fix.body, visited);
        default:
            fprintf(stderr, ERROR "Unknown expression type in occurs_in.\n" CRESET);
            exit(EXIT_FAILURE);
    }
}

bool occurs_in(Expression *var_or_hole, Expression *term) {
    return _occurs_in(var_or_hole, term, linear_map_new());
}

void fill_hole(Expression *hole, Expression *term) {
    if (hole->tag != HOLE_EXPRESSION) {
        return;
    }

    if (occurs_in(hole, term)) {
        return;
    }

    bool types_match = definitional_equal(get_expression_type(hole), get_expression_type(term));
    if (!types_match) {
        return;  // Todo: signal that this has failed?
    }
    DoublyLinkedList *holepars = hole->uplinks;
    for (int i = 0; i < dll_len(holepars); i++) {
        Uplink *uplink = dll_at(holepars, i)->data;
        switch (uplink->relation) {
            case (LAMBDA_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_LAMBDA_BODY(ptr, term);
                break;
            }
            case (LAMBDA_BOUND_VAR): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_LAMBDA_BOUND_VAR(ptr, term);
                break;
            }
            case (APP_FUNC): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_APP_FUNC(ptr, term);
                break;
            }
            case (APP_ARG): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_APP_ARG(ptr, term);
                break;
            }
            case (FORALL_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_FORALL_BODY(ptr, term);
                break;
            }
            case (FORALL_BOUND_VAR): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_FORALL_BOUND_VAR(ptr, term);
                break;
            }
            case (VAR_BODY): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_VAR_BODY(ptr, term);
                break;
            }
            case (EXPR_TYPE): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_EXPR_TYPE(ptr, term);
                break;
            }
            case (EXPR_CONTEXT): {
                Expression *ptr = (Expression *)uplink->ptr;
                SET_EXPR_CONTEXT(ptr, term);
                break;
            }
            default:
                fprintf(stderr, WARNING "todo: fill_hole for relation %d.\n" CRESET,
                        uplink->relation);
                break;
        }
    }
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
                return _congruence2(a->as.app.func, b->as.app.func, mapping) &&
                       _congruence2(a->as.app.arg, b->as.app.arg, mapping);
            case (FORALL_EXPRESSION): {
                linear_map_set(mapping, a->as.forall.bound_variable, b->as.forall.bound_variable);
                return _congruence2(a->as.forall.body, b->as.forall.body, mapping);
            }
            case (LAMBDA_EXPRESSION): {
                linear_map_set(mapping, a->as.lambda.bound_variable, b->as.lambda.bound_variable);
                return _congruence2(a->as.lambda.body, b->as.lambda.body, mapping);
            }
            case (VAR_EXPRESSION): {
                return (a == b) || (linear_map_get(mapping, a) == b);
            }
            case (HOLE_EXPRESSION): {
                return (a == b) || (linear_map_get(mapping, a) == b);
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