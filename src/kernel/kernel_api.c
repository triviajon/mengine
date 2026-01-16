#include "src/kernel/kernel_api.h"

#include <stdlib.h>

#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/definitional_equal.h"
#include "src/kernel/delta_reduction.h"
#include "src/kernel/expression.h"
#include "src/kernel/fix_reduction.h"
#include "src/kernel/inductive.h"
#include "src/kernel/iota_reduction.h"
#include "src/kernel/normalize.h"
#include "src/kernel/subst.h"
#include "src/kernel/utils.h"

/* EXPRESSION CONSTRUCTION */

Expression *kernel_var_create(const char *name, Expression *type, Context *context) {
    return init_var_expression_wc(name, type, context);
}

Expression *kernel_var_create_with_body(const char *name, Expression *body, Context *context) {
    return init_var_expression_wc_with_body(name, body, context);
}

Expression *kernel_app_create(Expression *func, Expression *arg, Context *context) {
    return init_app_expression_wc(func, arg, context);
}

Expression *kernel_lambda_create(Expression *bound_var, Expression *body) {
    return init_lambda_expression_wc(bound_var, body);
}

Expression *kernel_forall_create(Expression *bound_var, Expression *body) {
    return init_forall_expression_wc(bound_var, body);
}

Expression *kernel_arrow_create(Expression *lhs, Expression *rhs, Context *context) {
    return init_arrow_expression_wc(lhs, rhs, context);
}

Expression *kernel_hole_create(char *name, Expression *return_type, Context *context) {
    return init_hole_expression(name, return_type, context);
}

void *kernel_match_branch_create(Expression *constructor, Expression **pattern_variables,
                                 int pattern_var_count, Expression *body) {
    MatchBranch *branch = malloc(sizeof(MatchBranch));
    if (!branch) {
        return NULL;
    }

    branch->constructor = constructor;
    branch->pattern_variables = pattern_variables;
    branch->pattern_var_count = pattern_var_count;
    branch->body = body;

    return branch;
}

void kernel_match_branch_free(void *branch_) {
    MatchBranch *branch = (MatchBranch *)branch_;
    if (!branch) {
        return;
    }
    free(branch->pattern_variables);
    free(branch);
}

Expression *kernel_match_create(Expression *scrutinee, void **branches, int branch_count,
                                Context *context) {
    return init_match_expression_wc(scrutinee, (MatchBranch **)branches, branch_count, context);
}

Expression *kernel_fix_create(Expression *recursive_var, Expression **args, int arg_count,
                              Expression *body, int decreasing_arg_index, Context *context) {
    (void)context; /* context not used by init_fix_expression_wc */
    return init_fix_expression_wc(recursive_var, args, arg_count, decreasing_arg_index, body);
}

Expression *kernel_type_create(void) { return init_type_expression(); }

Expression *kernel_prop_create(void) { return init_prop_expression(); }

/* EXPRESSION INSPECTION */

Expression *kernel_expr_type(Expression *expr) { return get_expression_type(expr); }

Context *kernel_expr_context(Expression *expr) { return get_expression_context(expr); }

char *kernel_hole_name(Expression *hole_expr) { return get_hole_name(hole_expr); }

char *kernel_var_name(Expression *var_expr) { return get_var_name(var_expr); }

Expression *kernel_var_body(Expression *var_expr) { return get_var_body(var_expr); }

Expression *kernel_app_func(Expression *app_expr) { return get_app_func(app_expr); }

Expression *kernel_app_arg(Expression *app_expr) { return get_app_arg(app_expr); }

Expression *kernel_lambda_var(Expression *lambda_expr) {
    return get_lambda_bound_variable(lambda_expr);
}

Expression *kernel_lambda_body(Expression *lambda_expr) { return get_lambda_body(lambda_expr); }

Expression *kernel_forall_var(Expression *forall_expr) {
    return get_forall_bound_variable(forall_expr);
}

Expression *kernel_forall_body(Expression *forall_expr) { return get_forall_body(forall_expr); }

Expression *kernel_match_scrutinee(Expression *match_expr) {
    return get_match_scrutinee(match_expr);
}

Expression *kernel_fix_recursive_var(Expression *fix_expr) {
    return get_fix_recursive_var(fix_expr);
}

Expression **kernel_fix_args(Expression *fix_expr, int *out_count) {
    *out_count = get_fix_arg_count(fix_expr);
    return get_fix_args(fix_expr);
}

Expression *kernel_fix_body(Expression *fix_expr) { return get_fix_body(fix_expr); }

int kernel_fix_decreasing_arg(Expression *fix_expr) {
    return get_fix_decreasing_arg_index(fix_expr);
}

bool kernel_expr_is_hole(Expression *expr) { return is_hole(expr); }

bool kernel_expr_has_holes(Expression *expr) { return has_holes(expr); }

bool kernel_hole_fill(Expression *hole, Expression *term) {
    if (!hole || !term) {
        return false;
    }
    if (!can_fill(hole, term)) {
        return false;
    }
    fill_hole(hole, term);
    return true;
}

/* CONTEXT OPERATIONS */

Context *kernel_context_empty(void) { return context_create_empty(); }

Context *kernel_context_add(Context *context, Expression *var_expr) {
    // In this system, adding a variable to a context means the variable
    // expression itself becomes the new context. The variable should already
    // have been created with the given context as its parent.
    (void)context;  // The context is implicitly in var_expr
    return var_expr;
}

bool kernel_context_has_name(Context *context, char *name) {
    return context_contains_name(context, name);
}

Expression *kernel_context_lookup(Context *context, char *name) {
    return context_lookup_by_name(context, name);
}

int kernel_context_size(Context *context) { return context_size(context); }

bool kernel_context_is_ancestor(Context *context_a, Context *context_b) {
    return context_is_ancestor(context_a, context_b);
}

Context *kernel_context_cut(Context *context, Expression *x, Expression *a) {
    return context_cut(context, x, a);
}

/* REDUCTION OPERATIONS */

bool kernel_can_beta_reduce(Expression *func, Expression *arg) {
    return forms_beta_redex(func, arg);
}

Expression *kernel_beta_reduce(Context *context, Expression *func, Expression *arg) {
    return beta_reduce(context, func, arg);
}

bool kernel_can_delta_reduce(Expression *expr) { return is_delta_reducible(expr); }

Expression *kernel_delta_reduce(Expression *expr) { return delta_reduce(expr); }

bool kernel_can_iota_reduce(Expression *match_expr) { return is_iota_reducible(match_expr); }

Expression *kernel_iota_reduce(Context *context, Expression *match_expr) {
    return iota_reduce(context, match_expr);
}

bool kernel_can_fix_reduce(Expression *fix_expr) { return is_fix_reducible(fix_expr); }

Expression *kernel_fix_reduce(Expression *fix_expr) { return fix_reduce(fix_expr); }

/* SUBSTITUTION OPERATIONS */

Expression *kernel_subst(Context *context, Expression *t, Expression *x, Expression *a) {
    return new_subst(context, t, x, a);
}

Expression *kernel_p_subst(Context *context, Expression *t, void *old_exprs_list,
                           void *new_exprs_list) {
    return new_p_subst(context, t, (DoublyLinkedList *)old_exprs_list,
                       (DoublyLinkedList *)new_exprs_list);
}

/* NORMALIZATION OPERATIONS */

Expression *kernel_normalize_cbv(Expression *expr, unsigned int flags) {
    return normalize_cbv(expr, (ReductionFlags)flags);
}

Expression *kernel_normalize_compute(Expression *expr) { return normalize_compute(expr); }

Expression *kernel_normalize_whnf(Expression *expr) { return normalize_whnf(expr); }

/* TYPE CHECKING AND EQUALITY */

bool kernel_expr_valid_in_context(Expression *expr, Context *context) {
    return valid_in_context(expr, context);
}

bool kernel_expr_valid_to_add(Expression *expr, Context *context) {
    return valid_to_add_to_context(expr, context);
}

bool kernel_expr_definitionally_equal(Expression *a, Expression *b) {
    return definitional_equal(a, b);
}

bool kernel_expr_congruent(Expression *a, Expression *b) { return congruence(a, b); }

/* INDUCTIVE TYPE OPERATIONS */

bool kernel_expr_is_inductive(Expression *expr) { return is_inductive(expr); }

bool kernel_expr_is_constructor(Expression *expr) { return is_constructor(expr); }

Expression **kernel_inductive_constructors(Expression *inductive_var, int *out_count) {
    return get_constructors(inductive_var, out_count);
}

Expression *kernel_inductive_eliminator(Expression *inductive_var) {
    return get_eliminator(inductive_var);
}

bool kernel_expr_is_constructor_of(Expression *expr, Expression *inductive_var) {
    return is_constructor_of(expr, inductive_var);
}

bool kernel_inductive_register(Expression *inductive_var, Expression **constructors,
                               int constructor_count, Expression *eliminator) {
    return register_inductive(inductive_var, constructors, constructor_count, eliminator);
}

/* STRING CONVERSION */

char *kernel_expr_to_string(Expression *expr) { return stringify_expression(expr); }

char *kernel_context_to_string(Context *context) {
    return stringify_context(context, CTX_STRINGIFY_PRETTY_IND2);
}
