#ifndef KERNEL_API_H
#define KERNEL_API_H

// kernel_api.h - the term construction language API

#include <stdbool.h>
#include <stddef.h>

// Opaque types
typedef struct Expression Expression;
typedef struct Expression Context;

/* ============================================================================
 * Expression Construction
 * ============================================================================ */

Expression *kernel_var_create(const char *name, Expression *type, Context *context);
Expression *kernel_var_create_with_body(const char *name, Expression *body, Context *context);
Expression *kernel_app_create(Expression *func, Expression *arg, Context *context);
Expression *kernel_lambda_create(Expression *bound_var, Expression *body);
Expression *kernel_forall_create(Expression *bound_var, Expression *body);
Expression *kernel_arrow_create(Expression *lhs, Expression *rhs, Context *context);
Expression *kernel_hole_create(char *name, Expression *return_type, Context *context);
void *kernel_match_branch_create(Expression *constructor, Expression **pattern_variables,
                                 int pattern_var_count, Expression *body);
void kernel_match_branch_free(void *branch);
Expression *kernel_match_create(Expression *scrutinee, void **branches, int branch_count,
                                Context *context);
Expression *kernel_fix_create(Expression *recursive_var, Expression **args, int arg_count,
                              Expression *body, int decreasing_arg_index, Context *context);
Expression *kernel_type_create(void);
Expression *kernel_prop_create(void);

/* ============================================================================
 * Expression Inspection
 * ============================================================================ */

Expression *kernel_expr_type(Expression *expr);
Context *kernel_expr_context(Expression *expr);
char *kernel_hole_name(Expression *hole_expr);
char *kernel_var_name(Expression *var_expr);
Expression *kernel_var_body(Expression *var_expr);
Expression *kernel_app_func(Expression *app_expr);
Expression *kernel_app_arg(Expression *app_expr);
Expression *kernel_lambda_var(Expression *lambda_expr);
Expression *kernel_lambda_body(Expression *lambda_expr);
Expression *kernel_forall_var(Expression *forall_expr);
Expression *kernel_forall_body(Expression *forall_expr);
Expression *kernel_match_scrutinee(Expression *match_expr);
Expression *kernel_fix_recursive_var(Expression *fix_expr);
Expression **kernel_fix_args(Expression *fix_expr, int *out_count);
Expression *kernel_fix_body(Expression *fix_expr);
int kernel_fix_decreasing_arg(Expression *fix_expr);
bool kernel_expr_is_hole(Expression *expr);
bool kernel_expr_has_holes(Expression *expr);
bool kernel_hole_fill(Expression *hole, Expression *term);

/* ============================================================================
 * Context Operations
 * ============================================================================ */

Context *kernel_context_empty(void);
Context *kernel_context_add(Context *context, Expression *var_expr);
bool kernel_context_has_name(Context *context, char *name);
Expression *kernel_context_lookup(Context *context, char *name);
int kernel_context_size(Context *context);
bool kernel_context_is_ancestor(Context *context_a, Context *context_b);
Context *kernel_context_cut(Context *context, Expression *x, Expression *a);

/* ============================================================================
 * Reduction Operations
 * ============================================================================ */

bool kernel_can_beta_reduce(Expression *func, Expression *arg);
Expression *kernel_beta_reduce(Context *context, Expression *func, Expression *arg);
bool kernel_can_delta_reduce(Expression *expr);
Expression *kernel_delta_reduce(Expression *expr);
bool kernel_can_iota_reduce(Expression *match_expr);
Expression *kernel_iota_reduce(Context *context, Expression *match_expr);
bool kernel_can_fix_reduce(Expression *fix_expr);
Expression *kernel_fix_reduce(Expression *fix_expr);

/* ============================================================================
 * Substitution Operations
 * ============================================================================ */

/* Single substitution: t[x := a], result valid in context_cut(context, x, a) */
Expression *kernel_subst(Context *context, Expression *t, Expression *x, Expression *a);

/* Parallel substitution: replace multiple variables simultaneously */
Expression *kernel_p_subst(Context *context, Expression *t, void *old_exprs_list,
                           void *new_exprs_list);

/* ============================================================================
 * Normalization Operations
 * ============================================================================ */

#define KERNEL_REDUCE_BETA (1 << 0)
#define KERNEL_REDUCE_DELTA (1 << 1)
#define KERNEL_REDUCE_IOTA (1 << 2)
#define KERNEL_REDUCE_FIX (1 << 3)
#define KERNEL_REDUCE_ALL \
    (KERNEL_REDUCE_BETA | KERNEL_REDUCE_DELTA | KERNEL_REDUCE_IOTA | KERNEL_REDUCE_FIX)

Expression *kernel_normalize_cbv(Expression *expr, unsigned int flags);
Expression *kernel_normalize_compute(Expression *expr);
Expression *kernel_normalize_whnf(Expression *expr);

/* ============================================================================
 * Type Checking and Equality
 * ============================================================================ */

bool kernel_expr_valid_in_context(Expression *expr, Context *context);
bool kernel_expr_valid_to_add(Expression *expr, Context *context);
bool kernel_expr_definitionally_equal(Expression *a, Expression *b);
bool kernel_expr_congruent(Expression *a, Expression *b);

/* ============================================================================
 * Inductive Type Operations
 * ============================================================================ */

bool kernel_expr_is_inductive(Expression *expr);
bool kernel_expr_is_constructor(Expression *expr);
Expression **kernel_inductive_constructors(Expression *inductive_var, int *out_count);
Expression *kernel_inductive_eliminator(Expression *inductive_var);
bool kernel_expr_is_constructor_of(Expression *expr, Expression *inductive_var);
bool kernel_inductive_register(Expression *inductive_var, Expression **constructors,
                               int constructor_count, Expression *eliminator);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

char *kernel_expr_to_string(Expression *expr);
char *kernel_context_to_string(Context *context);

#endif  // KERNEL_API_H
