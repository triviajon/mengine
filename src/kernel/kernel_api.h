#ifndef KERNEL_API_H
#define KERNEL_API_H

// kernel_api.h - the term construction language API

#include <stdbool.h>
#include <stddef.h>

#include "src/common/map.h"

// Opaque types
typedef struct Expression Expression;
typedef struct Expression Context;
typedef struct Conversion Conversion;

/* ============================================================================
 * Expression Construction
 * ============================================================================ */

/**
 * Create a variable expression in the given context with a given name and type. The name is only
 * used for printing purposes.
 * Time Complexity: 
 * - Check if type is valid in context -> O(valid_in_context(type, context))
 * - Propogate evar refs -> O(propagate_evar_refs(expr, type))
 * - Insert into global context tree -> O(order_insert(context, new_var))
 *
 * @param name
 * @param type
 * @param context
 * @return
 */
Expression *kernel_var_create(const char *name, Expression *type, Context *context);

/**
 * Create a variable expression with an associated body in the given context.
 * Time Complexity:
 * - Check if body is valid in context -> O(valid_in_context(body, context))
 * - Check if body type is valid in context -> O(valid_in_context(type(body), context))
 * - Check if type(body) is a sort -> O(1)
 * - Propagate evar refs from type(body) -> O(propagate_evar_refs(expr, type(body)))
 * - Attach body and propagate evar refs from body -> O(propagate_evar_refs(expr, body))
 * - Insert into global context tree -> O(order_insert(context, new_var))
 *
 * @param name
 * @param body
 * @param context
 * @return
 */
Expression *kernel_var_create_with_body(const char *name, Expression *body, Context *context);

/**
 * Create an application expression (func applied to arg) in the given context.
 * Time Complexity: 
 * - Check if func is valid in context -> O(valid_in_context(func, context))
 * - Check if func type is a forall -> O(1)
 * - Check if arg is valid in context -> O(valid_in_context(arg, context))
 * - Construct app type -> O(_construct_app_type(context, func, arg)) # TODO
 * - Propagate evar refs from type -> O(propagate_evar_refs(expr, type))
 * - Propagate evar refs from func and arg -> O(propagate_evar_refs(expr, func) + propagate_evar_refs(expr, arg))
 *
 * @param func
 * @param arg
 * @param context
 * @return
 */
Expression *kernel_app_create(Expression *func, Expression *arg, Context *context);

/**
 * Create a lambda expression with a bound variable and body.
 * Time Complexity: 
 * - Check if body is valid in extended context -> O(valid_in_context(body, extended_with_bound_variable))
 * - Construct lambda type -> O(kernel_forall_create(bound_variable, get_expression_type(body)))
 * - Propagate evar refs from type -> O(propagate_evar_refs(expr, type))
 * - Propagate evar refs from bound variable and body -> O(propagate_evar_refs(expr, bound_variable) + propagate_evar_refs(expr, body))
 *
 * @param bound_var
 * @param body
 * @return
 */
Expression *kernel_lambda_create(Expression *bound_var, Expression *body);

/**
 * Create a forall expression with a bound variable and body.
 * Time Complexity:
 * - Check if body is valid in extended context -> O(valid_in_context(body, extended_with_bound_variable))
 * - If body type is Type, check if bound variable type is valid in context -> O(valid_in_context(type(bound_variable), context))
 * - Propagate evar refs from type -> O(propagate_evar_refs(expr, type))
 * - Propagate evar refs from bound variable and body -> O(propagate_evar_refs(expr, bound_variable) + propagate_evar_refs(expr, body))
 *
 * @param bound_var
 * @param body
 * @return
 */
Expression *kernel_forall_create(Expression *bound_var, Expression *body);

/**
 * Create an arrow expression (lhs -> rhs) in the given context.
 * Time Complexity: 
 * - Create anonymous variable for lhs -> O(kernel_var_create("_", lhs, context))
 * - Create forall over rhs using that anonymous variable -> O(kernel_forall_create(anonymous_variable, rhs))
 *
 * @param lhs
 * @param rhs
 * @param context
 * @return
 */
Expression *kernel_arrow_create(Expression *lhs, Expression *rhs, Context *context);

/**
 * Create a hole expression with a name and return type in the given context.
 * Time Complexity:
 * - Check if return_type is valid in context -> O(valid_in_context(return_type, context))
 * - Propagate evar refs from return_type -> O(propagate_evar_refs(expr, return_type))
 * - Add the hole to its own evar refs -> O(1)
 *
 * @param name
 * @param return_type
 * @param context
 * @return
 */
Expression *kernel_hole_create(char *name, Expression *return_type, Context *context);

/**
 * Create a match-branch object for use with kernel_match_create.
 * Time Complexity: constant
 *
 * @param constructor
 * @param pattern_variables
 * @param pattern_var_count
 * @param body
 * @return
 */
void *kernel_match_branch_create(Expression *constructor, Expression **pattern_variables,
                                 int pattern_var_count, Expression *body);

/**
 * Free a match-branch object previously created by kernel_match_branch_create.
 * Time Complexity: constant
 *
 * @param branch
 * @return
 */
void kernel_match_branch_free(void *branch);

/**
 * Free a kernel expression via the uplink-based ref-counting teardown.
 * Suitable for freeing the top-level runtime context on shutdown.
 * Time Complexity: constant (because we defer to shutdown right now)
 *
 * @param expr
 * @return
 */
void kernel_expr_free(Expression *expr);

/**
 * Free an expression (and all its unique descendants) without touching nodes
 * reachable from ctx.  Use instead of kernel_expr_free when the expression
 * shares children (e.g. context variables) with the live context chain.
 * Pass NULL for b when freeing a single expression.
 * Time Complexity: constant (because we defer to shutdown right now)
 *
 * @param a
 * @param b
 * @param ctx
 * @return
 */
void kernel_expr_free_excluding_ctx(Expression *a, Expression *b, Context *ctx);

/**
 * Free a filled hole expression without cascading into children.
 * Only safe to call on holes that have been filled via kernel_hole_fill.
 * Time Complexity: constant (because we defer to shutdown right now)
 *
 * @param hole
 * @return
 */
void kernel_free_filled_hole(Expression *hole);

/**
 * Free the context chain from leaf to root. Since context is a non-owning
 * edge, the chain must be freed explicitly. Stops at EMPTY_CONTEXT.
 * Time Complexity: constant (because we defer to shutdown right now)
 *
 * @param ctx
 * @return
 */
void kernel_context_free(Context *ctx);

/**
 * Shut down kernel-owned expression GC bookkeeping during runtime teardown.
 * Time Complexity: O(number of expressions allocated)
 *
 * @return
 */
void kernel_expression_gc_shutdown(void);

/**
 * Return true if expr is one of the nodes directly in the context chain,
 * i.e. reachable from ctx by following ->context pointers.
 * Time Complexity:
 * - O(context_is_ancestor(expr, ctx))
 *
 * @param ctx
 * @param expr
 * @return
 */
bool kernel_context_contains(Context *ctx, Expression *expr);

/**
 * Create a match expression from a scrutinee and a list of branches.
 * Time Complexity:
 *
 * @param scrutinee
 * @param branches
 * @param branch_count
 * @param context
 * @return
 */
Expression *kernel_match_create(Expression *scrutinee, void **branches, int branch_count,
                                Context *context);

/**
 * Create a fix expression (recursive definition) in the given context.
 * Time Complexity:
 * - Count expected arguments from recursive_var's forall type -> O(number_of_forall_args(type(recursive_var)))
 * - Check if body is valid in the recursive context extended by args -> O(valid_in_context(body, extended_context))
 * - Check structural recursion on the decreasing argument -> O(check_all_recursive_calls(body, recursive_var, decreasing_arg))
 * - Close body over args with lambdas -> O(arg_count * kernel_lambda_create(arg_i, body_bound_i))
 * - Propagate evar refs from type, recursive_var, args, and body -> O(propagate_evar_refs(expr, type(recursive_var)) + propagate_evar_refs(expr, recursive_var) + sum_i propagate_evar_refs(expr, args[i]) + propagate_evar_refs(expr, body))
 * - Register closed body on recursive_var -> O(register_fix_body_to_expression(recursive_var, body_bound))
 *
 * @param recursive_var
 * @param args
 * @param arg_count
 * @param body
 * @param decreasing_arg_index
 * @param context
 * @return
 */
Expression *kernel_fix_create(Expression *recursive_var, Expression **args, int arg_count,
                              Expression *body, int decreasing_arg_index, Context *context);

/**
 * Create the universe Type expression.
 * Time Complexity: constant
 *
 * @return
 */
Expression *kernel_type_create(void);

/**
 * Create the Prop expression.
 * Time Complexity: constant
 *
 * @return
 */
Expression *kernel_prop_create(void);

/* ============================================================================
 * Expression Inspection
 * ============================================================================ */

/**
 * Get the type of an expression.
 * Time Complexity: constant
 *
 * @param expr
 * @return
 */
Expression *kernel_expr_type(Expression *expr);

/**
 * Get the context of an expression.
 * Time Complexity: constant
 *
 * @param expr
 * @return
 */
Context *kernel_expr_context(Expression *expr);

/**
 * Get the name stored in a hole expression.
 * Time Complexity: constant
 *
 * @param hole_expr
 * @return
 */
char *kernel_hole_name(Expression *hole_expr);

/**
 * Get the name of a variable expression.
 * Time Complexity: constant
 *
 * @param var_expr
 * @return
 */
char *kernel_var_name(Expression *var_expr);

/**
 * Get the body of a variable expression (if any).
 * Time Complexity: constant
 *
 * @param var_expr
 * @return
 */
Expression *kernel_var_body(Expression *var_expr);

/**
 * Get the function part of an application expression.
 * Time Complexity: constant
 *
 * @param app_expr
 * @return
 */
Expression *kernel_app_func(Expression *app_expr);

/**
 * Get the argument part of an application expression.
 * Time Complexity: constant
 *
 * @param app_expr
 * @return
 */
Expression *kernel_app_arg(Expression *app_expr);

/**
 * Get the bound variable of a lambda expression.
 * Time Complexity: constant
 *
 * @param lambda_expr
 * @return
 */
Expression *kernel_lambda_var(Expression *lambda_expr);

/**
 * Get the body of a lambda expression.
 * Time Complexity: constant
 *
 * @param lambda_expr
 * @return
 */
Expression *kernel_lambda_body(Expression *lambda_expr);

/**
 * Get the bound variable of a forall expression.
 * Time Complexity: constant
 *
 * @param forall_expr
 * @return
 */
Expression *kernel_forall_var(Expression *forall_expr);

/**
 * Get the body of a forall expression.
 * Time Complexity: constant
 *
 * @param forall_expr
 * @return
 */
Expression *kernel_forall_body(Expression *forall_expr);

/**
 * Get the scrutinee of a match expression.
 * Time Complexity: constant
 *
 * @param match_expr
 * @return
 */
Expression *kernel_match_scrutinee(Expression *match_expr);

/**
 * Get the recursive variable of a fix expression.
 * Time Complexity: constant
 *
 * @param fix_expr
 * @return
 */
Expression *kernel_fix_recursive_var(Expression *fix_expr);

/**
 * Get the arguments of a fix expression.
 * Time Complexity: constant
 *
 * @param fix_expr
 * @param out_count
 * @return
 */
Expression **kernel_fix_args(Expression *fix_expr, int *out_count);

/**
 * Get the body of a fix expression.
 * Time Complexity: constant
 *
 * @param fix_expr
 * @return
 */
Expression *kernel_fix_body(Expression *fix_expr);

/**
 * Get the decreasing argument index of a fix expression.
 * Time Complexity: constant
 *
 * @param fix_expr
 * @return
 */
int kernel_fix_decreasing_arg(Expression *fix_expr);

/**
 * Check whether an expression is a hole.
 * Time Complexity: constant
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_hole(Expression *expr);

/**
 * Check whether an expression is a variable.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_var(Expression *expr);

/**
 * Check whether an expression is Type.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_type(Expression *expr);

/**
 * Check whether an expression is Prop.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_prop(Expression *expr);

/**
 * Check whether an expression is an application.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_app(Expression *expr);

/**
 * Check whether an expression is a lambda.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_lambda(Expression *expr);

/**
 * Check whether an expression is a forall.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_forall(Expression *expr);

/**
 * Check whether an expression is a match expression.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_match(Expression *expr);

/**
 * Check whether an expression is a fix expression.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_fix(Expression *expr);

/**
 * Check whether a hole has already been filled or shelved.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param hole
 * @return
 */
bool kernel_hole_is_satisfied(Expression *hole);

/**
 * Mark a hole as satisfied without filling it. This is used by the proof
 * engine for dependent evars that will be filled by another open goal.
 * Time Complexity: constant
 *
 * Space Complexity:
 *
 * @param hole
 * @return
 */
void kernel_hole_mark_satisfied(Expression *hole);

/**
 * Check whether an expression contains any holes.
 * Time Complexity: constant
 *
 * @param expr
 * @return
 */
bool kernel_expr_has_holes(Expression *expr);

/**
 * Fill a hole with a term if the fill is valid.
 * Time Complexity:
 * - Check if term is valid in hole's context -> O(valid_in_context(term, context(hole)))
 * - Check if hole type and term type are compatible, collecting sub-hole assignments -> O(open_types_compatible_collecting_in_context(context(hole), type(hole), type(term), assignments))
 * - Check that term does not contain hole -> O(expr_refs_hole(term, hole))
 * - Fill collected sub-holes -> O(sum_i kernel_hole_fill(assignment_holes[i], assignment_terms[i]))
 * - Clear conversion cache -> O(conversion_cache_clear())
 * - Rewrite direct structural uplinks from hole to term -> O(uplink_count(hole))
 * - Recompute evar refs for hole and affected ancestors -> O(sum_{a in affected_ancestors} recompute_evar_refs(a))
 *
 *
 * @param hole
 * @param term
 * @return
 */
bool kernel_hole_fill(Expression *hole, Expression *term);

/* ============================================================================
 * Context Operations
 * ============================================================================ */

/**
 * Create or return the empty context.
 * Time Complexity: constant
 *
 * @return
 */
Context *kernel_context_empty(void);

/**
 * Check whether a context contains a binding with the given name.
 * Time Complexity: O(|context|)
 *
 * @param context
 * @param name
 * @return
 */
bool kernel_context_has_name(Context *context, char *name);

/**
 * Look up a binding by name in a context.
 * Time Complexity: O(|context|)
 *
 * @param context
 * @param name
 * @return
 */
Expression *kernel_context_lookup(Context *context, char *name);

/**
 * Get the size of a context.
 * Time Complexity: O(|context|)
 *
 * @param context
 * @return
 */
int kernel_context_size(Context *context);

/**
 * Check whether one context is an ancestor of another.
 * Time Complexity: O(order_precedes(context_a, context_b))
 *
 * @param context_a
 * @param context_b
 * @return
 */
bool kernel_context_is_ancestor(Context *context_a, Context *context_b);

/**
 * Apply the cut rule to a context for substitution validity.
 * Time Complexity:
 * - Check if x occurs in context -> O(context_find(context, x))
 * - For each binding v: T in the suffix after x:
 *   + Substitute T[x := a] -> O(_subst(cut_prefix, T, x, a))
 *   + Create the rebuilt context variable -> O(kernel_var_create(name(v), T[x := a], cut_prefix))
 *
 * @param context
 * @param x
 * @param a
 * @return
 */
Context *kernel_context_cut(Context *context, Expression *x, Expression *a);

/* ============================================================================
 * Reduction Operations
 * ============================================================================ */

/**
 * Check whether func applied to arg forms a beta-redex.
 * Time Complexity: 
 * - Check whether func is a lambda and arg exists -> O(1)
 *
 * @param func
 * @param arg
 * @return
 */
bool kernel_can_beta_reduce(Expression *func, Expression *arg);

/**
 * Perform beta reduction on a function application.
 * Time Complexity:
 * - Substitute the application argument for the lambda-bound variable in the lambda body -> O(beta_subst(context, body(func), bound_variable(func), arg))
 *
 * @param context
 * @param func
 * @param arg
 * @return
 */
Expression *kernel_beta_reduce(Context *context, Expression *func, Expression *arg);

/**
 * Check whether an expression is delta-reducible.
 * Time Complexity:
 * - Check whether expr is a variable with a body -> O(1)
 *
 * @param expr
 * @return
 */
bool kernel_can_delta_reduce(Expression *expr);

/**
 * Perform delta reduction on an expression.
 * Time Complexity:
 * - Check whether expr is delta-reducible -> O(kernel_can_delta_reduce(expr))
 * - Return the stored variable body -> O(1)
 *
 * @param expr
 * @return
 */
Expression *kernel_delta_reduce(Expression *expr);

/**
 * Check whether a match expression is iota-reducible.
 * Time Complexity:
 * - Check whether match scrutinee has an inductive type -> O(is_inductive(type(scrutinee)))
 * - Find the head of the scrutinee application spine -> O(application_spine_height(scrutinee))
 * - Check whether that head is a constructor of the inductive type -> O(is_constructor_of(head(scrutinee), type(scrutinee)))
 *
 * @param match_expr
 * @return
 */
bool kernel_can_iota_reduce(Expression *match_expr);

/**
 * Perform iota reduction on a match expression.
 * Time Complexity:
 * - Check whether match_expr is iota-reducible -> O(kernel_can_iota_reduce(match_expr))
 * - Extract scrutinee application arguments -> O(application_spine_height(scrutinee))
 * - Find the matching branch -> O(sum_i congruence(branch_constructor_i, head(scrutinee)))
 * - Substitute scrutinee arguments for branch pattern variables in the matching branch body -> O(new_p_subst(context, branch_body, pattern_subst_map))
 *
 * @param context
 * @param match_expr
 * @return
 */
Expression *kernel_iota_reduce(Context *context, Expression *match_expr);

/**
 * Check whether a fix expression is reducible.
 * Time Complexity:
 * - Check whether fix_expr is a fix expression -> O(1)
 *
 * @param fix_expr
 * @return
 */
bool kernel_can_fix_reduce(Expression *fix_expr);

/**
 * Perform fix reduction on a fix expression.
 * Time Complexity:
 * - Substitute fix_expr for its recursive variable in the fix body -> O(new_subst(context(body), body, recursive_var, fix_expr))
 * - Close the substituted body over fix arguments with lambdas -> O(arg_count * kernel_lambda_create(arg_i, body_bound_i))
 *
 * @param fix_expr
 * @return
 */
Expression *kernel_fix_reduce(Expression *fix_expr);

/* ============================================================================
 * Substitution Operations
 * ============================================================================ */

/* Single substitution: t[x := a], result valid in context_cut(context, x, a) */
/**
 * Perform a single substitution t[x := a] under the given context.
 * Time Complexity:
 * - Check whether substitution can be skipped using uplinks/context -> O(substitution_skip_by_uplinks(t, x))
 * - Cut x out of context and substitute through the context suffix -> O(kernel_context_cut(context, x, a))
 * - Run top-down parallel substitution over t. If s is the number of context
 *   binders after x and d is the maximum number of nested binders inside t, the
 *   substitution map has size O(s + d) during traversal -> O(kernel_p_subst(final_context, t, subst_map))
 *
 * @param context
 * @param t
 * @param x
 * @param a
 * @return
 */
Expression *kernel_subst(Context *context, Expression *t, Expression *x, Expression *a);

/* Parallel substitution: replace multiple variables simultaneously */
/**
 * Perform a parallel substitution replacing multiple variables simultaneously.
 * Time Complexity:
 * Let V be the expression occurrences reached before context-pruning stops the
 * traversal, and let d be the maximum number of nested binders inside t.
 * - Context-pruning scans binder prefixes, bounded by O(|V| * d) in the worst case.
 * - Rebuilding visited nodes calls the corresponding kernel constructors -> O(sum rebuild_cost(e)).
 * Overall: O(|V| * d + sum rebuild_cost(e)).
 *
 * @param context
 * @param t
 * @param old_exprs_list
 * @param new_exprs_list
 * @return
 */
Expression *kernel_p_subst(Context *context, Expression *t, Map *subst_map);

/* ============================================================================
 * Normalization Operations
 * ============================================================================ */

#define KERNEL_REDUCE_BETA (1 << 0)
#define KERNEL_REDUCE_DELTA (1 << 1)
#define KERNEL_REDUCE_IOTA (1 << 2)
#define KERNEL_REDUCE_FIX (1 << 3)
#define KERNEL_REDUCE_ALL \
    (KERNEL_REDUCE_BETA | KERNEL_REDUCE_DELTA | KERNEL_REDUCE_IOTA | KERNEL_REDUCE_FIX)

/**
 * Normalize an expression using call-by-value with selected reduction flags.
 * Time Complexity: TODO
 *
 * @param expr
 * @param flags
 * @return
 */
Expression *kernel_normalize_cbv(Expression *expr, unsigned int flags);

/**
 * Normalize an expression using the default compute strategy.
 * Time Complexity: TODO
 *
 * @param expr
 * @return
 */
Expression *kernel_normalize_compute(Expression *expr);

/**
 * Normalize an expression to weak head normal form.
 * Time Complexity: TODO
 *
 * @param expr
 * @return
 */
Expression *kernel_normalize_whnf(Expression *expr);

/* ============================================================================
 * Type Checking and Equality
 * ============================================================================ */

/*
 * Context ancestry cost depends on the order implementation:
 * - order_linkedlist: O(depth(context)) by walking parent pointers.
 * - order_demain: O(1) query using maintained Euler-tour tags; insertion/deletion
 *   maintenance is paid when context variables are created/freed.
 */

/**
 * Check whether an expression is valid in the given context.
 * Time Complexity:
 * - O(context_ancestry_query)
 *
 * @param expr
 * @param context
 * @return
 */
bool kernel_expr_valid_in_context(Expression *expr, Context *context);

/**
 * Check whether an expression can be added to the given context.
 * Time Complexity:
 * - O(valid_in_context(type(expr), context))
 *
 * @param expr
 * @param context
 * @return
 */
bool kernel_expr_valid_to_add(Expression *expr, Context *context);

/**
 * Get the body of the innermost lambda/forall tower.
 * Time Complexity: O(|expr|)
 *
 * @param expr
 * @return
 */
Expression *kernel_expr_innermost_body(Expression *expr);

/**
 * Get the head of an application spine.
 * Time Complexity: O(|expr|)
 *
 * @param expr
 * @return
 */
Expression *kernel_expr_head(Expression *expr);

/**
 * Get the left side of an arrow represented as a forall.
 * Time Complexity: O(1)
 *
 * @param expr
 * @return
 */
Expression *kernel_arrow_lhs(Expression *expr);

/**
 * Get the right side of an arrow represented as a forall.
 * Time Complexity: O(1)
 *
 * @param expr
 * @return
 */
Expression *kernel_arrow_rhs(Expression *expr);

/**
 * Return conversion evidence usable in a particular context, or NULL.
 * The returned object is owned by the caller.
 * Time Complexity:
 * - O(valid_in_context(a, context) + valid_in_context(b, context))
 * - O(conversion_derivable(a, b))
 * - O(context_ancestry_query)
 *
 * @param context
 * @param a
 * @param b
 * @return
 */
Conversion *kernel_expr_conversion_in_context(Context *context, Expression *a, Expression *b);

/**
 * Free conversion evidence returned by the kernel conversion API.
 * Time Complexity: O(|conv|)
 *
 * @param conv
 * @return
 */
void kernel_conversion_free(Conversion *conv);

/**
 * Return true iff conversion evidence can be used in the supplied context.
 * Time Complexity: O(context_ancestry_query)
 *
 * @param conv
 * @param context
 * @return
 */
bool kernel_conversion_valid_in_context(Conversion *conv, Context *context);

/**
 * Check whether two expressions are congruent.
 * Time Complexity: O(|a| + |b|)
 *
 * @param a
 * @param b
 * @return
 */
bool kernel_expr_congruent(Expression *a, Expression *b);

/* ============================================================================
 * Inductive Type Operations
 * ============================================================================ */

/**
 * Check whether an expression is an inductive type.
 * Time Complexity: TODO
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_inductive(Expression *expr);

/**
 * Check whether an expression is a constructor.
 * Time Complexity: TODO
 *
 * @param expr
 * @return
 */
bool kernel_expr_is_constructor(Expression *expr);

/**
 * Get the constructors of an inductive type.
 * Time Complexity: TODO
 *
 * @param inductive_var
 * @param out_count
 * @return
 */
Expression **kernel_inductive_constructors(Expression *inductive_var, int *out_count);

/**
 * Get the eliminator for an inductive type.
 * Time Complexity: TODO
 *
 * @param inductive_var
 * @return
 */
Expression *kernel_inductive_eliminator(Expression *inductive_var);

/**
 * Check whether an expression is a constructor of a given inductive type.
 * Time Complexity: TODO
 *
 * @param expr
 * @param inductive_var
 * @return
 */
bool kernel_expr_is_constructor_of(Expression *expr, Expression *inductive_var);

/**
 * Register an inductive type and its constructors with the kernel.
 * Time Complexity: TODO
 *
 * @param inductive_var
 * @param constructors
 * @param constructor_count
 * @param eliminator
 * @return
 */
bool kernel_inductive_register(Expression *inductive_var, Expression **constructors,
                               int constructor_count, Expression *eliminator);

/* ============================================================================
 * String Conversion
 * ============================================================================ */

/**
 * Convert an expression to a string.
 * Time Complexity: O(2^{|expr|}) # TODO exponential in the worst case due to naive unfolding of recursive definitions
 *
 * @param expr
 * @return
 */
char *kernel_expr_to_string(Expression *expr);

/**
 * Convert a context to a string.
 * Time Complexity: O(|context|)
 *
 * @param context
 * @return
 */
char *kernel_context_to_string(Context *context);

/**
 * Convert a context to a string, stopping at the given context.
 * Time Complexity: O(depth(context))
 * @param context
 * @param until
 * @return
 */
char *kernel_context_to_string_until(Context *context, Context *until);

#endif  // KERNEL_API_H
