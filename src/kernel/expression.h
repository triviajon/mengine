#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "src/engine/rewrite_proof.h"
#include "src/kernel/doubly_linked_list.h"

// Forward declarations
typedef struct Expression Expression;
typedef struct Expression Context;

// Supported expression types for the Expression struct.
typedef enum {
    VAR_EXPRESSION,
    LAMBDA_EXPRESSION,
    APP_EXPRESSION,
    FORALL_EXPRESSION,
    TYPE_EXPRESSION,
    PROP_EXPRESSION,
    HOLE_EXPRESSION,
    MATCH_EXPRESSION,
    FIX_EXPRESSION,
} ExpressionType;

// Represents a parent-child relationship between expressions.
typedef enum {
    LAMBDA_BODY,               // expr->as.lambda.body
    LAMBDA_BOUND_VAR,          // expr->as.lambda.bound_variable
    APP_FUNC,                  // expr->as.app.func
    APP_ARG,                   // expr->as.app.arg
    FORALL_BODY,               // expr->as.forall.body
    FORALL_BOUND_VAR,          // expr->as.forall.bound_variable
    VAR_BODY,                  // expr->as.var.body
    EXPR_TYPE,                 // expr->type
    EXPR_CONTEXT,              // expr->context
    MATCH_SCRUTINEE,           // expr->as.match.scrutinee
    MATCH_BRANCH_BODY,         // expr->as.match.branches[i]->body
    MATCH_BRANCH_PATTERN_VAR,  // expr->as.match.branches[i]->pattern_variables[j]
    MATCH_BRANCH_CONSTRUCTOR,  // expr->as.match.branches[i]->constructor
    FIX_RECURSIVE_VAR,         // expr->as.fix.recursive_var
    FIX_ARG,                   // expr->as.fix.args[i]
    FIX_BODY,                  // expr->as.fix.body
} Relation;

/*
An uplink is a combination of
    1a) a pointer to a expression (one of my parents) OR
    1b) a pointer to a context (where I am referenced)
    2) an uplink relation (how what am I to that parent)

    Either 1a or 1b will ever be true, but never both.
*/
typedef struct {
    void *ptr;
    Relation relation;
} Uplink;

// A variable/type binding.
typedef struct {
    char *name;        // User-friendly name for the variable. Not used internally.
    Expression *body;  // If the variable is non-opaque, then this field
                       // will be non-NULL with the body of the variable
} VarExpression;

// A lambda expression: fun (bound_variable) => body.
typedef struct {
    Expression *bound_variable;  // The bound variable of the lambda.
    Expression *body;            // The body of the lamdbda expression.
} LambdaExpression;

// An application expression: (func arg).
typedef struct {
    Expression *func;   // The function which is applied to the argument. Has
                        // type Forall...
    Expression *arg;    // The argument being operating on.
    Expression *cache;  // A copied version of this application which is used in
                        // beta-reduction with Lambda-DAGs
} AppExpression;

// Similar to LambdaExpression.
typedef struct {
    Expression *bound_variable;
    Expression *body;
} ForallExpression;

typedef struct {
    // No additional fields beyond what's in Expression
} TypeExpression;

typedef struct {
    // No additional fields beyond what's in Expression
} PropExpression;

// TYPE and PROP are the only structs with the TypeExpression and PropExpression
// ExpressionTypes, i.e., they are singletons.
static Expression *TYPE = NULL;
static Expression *PROP = NULL;

// A typed hole to be filled later.
typedef struct {
    char *name;  // A user-friendly name for the hole. Not used internally.
} HoleExpression;

// A single branch in a match expression.
typedef struct {
    Expression *constructor;         // The constructor to match against
    Expression **pattern_variables;  // Bound variables from the pattern
    int pattern_var_count;           // Number of pattern variables
    Expression *body;                // Expression to evaluate if this branch matches
} MatchBranch;

// A match expression for pattern matching on inductive types.
typedef struct {
    Expression *scrutinee;   // Expression being matched on
    MatchBranch **branches;  // Array of match branches
    int branch_count;        // Number of branches
} MatchExpression;

// A fix expression: fix (x1: A1) (x2: A2) ... (xn: An) {struct xi} := body.
typedef struct {
    Expression *recursive_var;  // recursive_var : forall x1:A1, ..., xn:An, B
    Expression **args;          // Arguments of the fix expression
    int arg_count;              // Number of arguments
    int decreasing_arg_index;   // Index of the argument that is structurally decreasing
    Expression *body;           // The body of the fix expression
} FixExpression;

// Represents a generic expression.
struct Expression {
    // Common fields
    ExpressionType tag;         // The kind of expression (VAR, LAMBDA, APP, etc.)
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced
    Context *context;           // The minimal context this expression is valid in
                                // NULL for TYPE and PROP
    int ctx_size;               // Size of the context (0 for empty, 1+ for variables)
                                // 0 for TYPE and PROP
    Expression *type;           // The type of this expression
                                // NULL for TYPE and PROP
    bool maybe_hole_free;       // True means term is hole-free, false means may
                                // contain holes. Unused for TYPE and PROP.

    union {
        VarExpression var;
        LambdaExpression lambda;
        AppExpression app;
        ForallExpression forall;
        TypeExpression type_expr;
        PropExpression prop_expr;
        HoleExpression hole;
        MatchExpression match;
        FixExpression fix;
    } as;
};

// Helper function to add an uplink to the uplinks list of an expression.
void add_to_parents(Expression *expression, void *ptr, Relation r);

// Helper function to remove the first top_level_hole uplink from an
// expression's uplinks.
void remove_tl_uplink(Expression *expression);

// Remove a specific uplink from an expression's uplinks list.
void remove_uplink(Expression *expr, void *parent_ptr, Relation rel);

// Free an expression and recursively free children if they become unreachable.
// Never frees TYPE, PROP, or EMPTY_CONTEXT globals.
void free_expression(Expression *expr);

// Create a new uplink describing how ptr relates.
Uplink *new_uplink(void *ptr, Relation r);

// Macros for setting Expression fields
#define SET_EXPR_TAG(expr, value) (expr)->tag = (value);

#define SET_EXPR_UPLINKS(expr, value) (expr)->uplinks = (value);

#define SET_EXPR_CONTEXT(expr, value)                  \
    (expr)->context = (value);                         \
    if (!context_is_empty(value)) {                    \
        add_to_parents((value), (expr), EXPR_CONTEXT); \
    }

#define SET_EXPR_CTX_SIZE(expr, value) (expr)->ctx_size = (value);

#define SET_EXPR_TYPE(expr, value) \
    (expr)->type = (value);        \
    add_to_parents((value), (expr), EXPR_TYPE)

#define SET_EXPR_MAYBE_HOLE_FREE(expr, value) (expr)->maybe_hole_free = (value);

#define SET_HOLE_NAME(expr, value) (expr)->as.hole.name = (value);

#define SET_VAR_NAME(expr, value) (expr)->as.var.name = (value);

#define SET_VAR_BODY(expr, value)  \
    (expr)->as.var.body = (value); \
    add_to_parents((value), (expr), VAR_BODY)

#define SET_LAMBDA_BODY(expr, value)  \
    (expr)->as.lambda.body = (value); \
    add_to_parents((value), (expr), LAMBDA_BODY)

#define SET_LAMBDA_BOUND_VAR(expr, value)       \
    (expr)->as.lambda.bound_variable = (value); \
    add_to_parents((value), (expr), LAMBDA_BOUND_VAR)

#define SET_APP_FUNC(expr, value)  \
    (expr)->as.app.func = (value); \
    add_to_parents((value), (expr), APP_FUNC)

#define SET_APP_ARG(expr, value)  \
    (expr)->as.app.arg = (value); \
    add_to_parents((value), (expr), APP_ARG)

#define SET_FORALL_BODY(expr, value)  \
    (expr)->as.forall.body = (value); \
    add_to_parents((value), (expr), FORALL_BODY)

#define SET_FORALL_BOUND_VAR(expr, value)       \
    (expr)->as.forall.bound_variable = (value); \
    add_to_parents((value), (expr), FORALL_BOUND_VAR)

#define SET_MATCH_SCRUTINEE(expr, value)  \
    (expr)->as.match.scrutinee = (value); \
    add_to_parents((value), (expr), MATCH_SCRUTINEE)

#define SET_MATCH_BRANCHES(expr, value)                                                        \
    (expr)->as.match.branches = malloc((expr)->as.match.branch_count * sizeof(MatchBranch *)); \
    (expr)->as.match.branch_count = (expr)->as.match.branch_count;                             \
    (expr)->as.match.branches = (value);                                                       \
    for (int __i = 0; __i < (expr)->as.match.branch_count; __i++) {                            \
        MatchBranch *branch = (expr)->as.match.branches[__i];                                  \
        (expr)->as.match.branches[__i] = branch;                                               \
        add_to_parents(branch->constructor, (expr), MATCH_BRANCH_CONSTRUCTOR);                 \
        add_to_parents(branch->body, (expr), MATCH_BRANCH_BODY);                               \
        for (int __j = 0; __j < branch->pattern_var_count; __j++) {                            \
            add_to_parents(branch->pattern_variables[__j], (expr), MATCH_BRANCH_PATTERN_VAR);  \
        }                                                                                      \
    }

#define SET_FIX_RECURSIVE_VAR(expr, value)  \
    (expr)->as.fix.recursive_var = (value); \
    add_to_parents((value), (expr), FIX_RECURSIVE_VAR)

#define SET_FIX_ARGS(expr, value)                                                  \
    (expr)->as.fix.args = malloc((expr)->as.fix.arg_count * sizeof(Expression *)); \
    (expr)->as.fix.arg_count = (expr)->as.fix.arg_count;                           \
    (expr)->as.fix.args = (value);                                                 \
    for (int __i = 0; __i < (expr)->as.fix.arg_count; __i++) {                     \
        (expr)->as.fix.args[__i] = (value)[__i];                                   \
        add_to_parents((value)[__i], (expr), FIX_ARG);                             \
    }

#define SET_FIX_ARG_COUNT(expr, value) (expr)->as.fix.arg_count = (value);

#define SET_FIX_DECREASING_ARG_INDEX(expr, value) (expr)->as.fix.decreasing_arg_index = (value);

#define SET_FIX_BODY(expr, value)  \
    (expr)->as.fix.body = (value); \
    add_to_parents((value), (expr), FIX_BODY)

// Todo: We should consider adding context arguments to these functions?

// Create a new type expression.
Expression *init_type_expression();

// Create a new prop expression.
Expression *init_prop_expression();

// Create a new hole expression with a given name, return type, and context.
// Typing rule:
//    gamma |- return_type : s
//    s in {Prop, Type_i}
// ------------------------------------------------
//    gamma, name : return_type |-
Expression *init_hole_expression(char *name, Expression *return_type, Context *gamma);

// Create a new variable expression with a given name, type, and defining
// context. Typing rule:
//    gamma |- type : s
//    s in {Prop, Type_i}
// ------------------------------------------------
//    gamma, name : type |-
// In other words, if the type is valid in the input context, and the type(s) is
// a Prop/Type_i, then this variable is valid in the extension of the context.
Expression *init_var_expression_wc(const char *name, Expression *type, Context *gamma);

// Create a new variable expression with a given name, definition, and context.
// context. Typing rule:
//    gamma |- body : A
//    gamma |- A : a
//    s in {Prop, Type_i}
// ------------------------------------------------
//    gamma, x : A |-
// In other words, if the type is valid in the input context, and the type(s) is
// a Prop/Type_i, then this variable is valid in the extension of the context.
Expression *init_var_expression_wc_with_body(const char *name, Expression *body, Context *gamma);

// Create a new lambda/abstraction expression with a bound variable and body.
// Typing rule:
//     gamma, bound_variable : A |- body : B
// ------------------------------------------------
//     gamma |- fun (bound_variable: A) => body : Forall (bound_variable: A), B
// Where gamma = context(bound_variable).
Expression *init_lambda_expression_wc(Expression *bound_variable, Expression *body);

// Create a new application expression with a function, argument, and context.
// Typing rule:
//    gamma |- func : Forall bound_variable: A, B
//    gamma |- arg : A
// ------------------------------------------------
//    gamma |- func arg : B[bound_variable -> arg]
// The context parameter should typically be the longer of context(func) and context(arg).
Expression *init_app_expression_wc(Expression *func, Expression *arg, Context *context);

// Create a new forall expression with a bound variable and body.
// Typing rule:
//    gamma, bound_variable : A |- body : s, s in {Prop, Type_i}
//    (if s = Type_i, then) gamma |- A : s
// ------------------------------------------------
//    gamma |- Forall bound_variable: A, body : s
// Where gamma = context(bound_variable).
Expression *init_forall_expression_wc(Expression *bound_variable, Expression *body);

// Create a new arrow expression with a left-hand side, right-hand side, and context.
// The typing rule is a special case of the forall expression typing rule.
// Typing rule:
//    gamma, _ : lhs |- rhs : s, s in {Prop, Type_i}
//    (if s = Type_i, then) gamma |- lhs : s
// ------------------------------------------------
//    gamma |- Forall _: lhs, rhs : s (which is equivalent to "lhs -> rhs")
// The context parameter should typically be the longer of context(lhs) and context(rhs).
Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs, Context *gamma);

// Creates a match expression that pattern matches on an inductive type.
// Typing rule:
//    gamma |- scrutinee : I
//    For each branch i:
//      gamma, pattern_vars[i] : type(pattern_vars[i]) |- body[i] : T
// ------------------------------------------------
//    gamma |- match scrutinee with branches end : T
Expression *init_match_expression_wc(Expression *scrutinee, MatchBranch **branches,
                                     int branch_count, Context *context);

// Create a new fix expression with a given arguments, struct argument, and body.
// Typing rule:
//    gamma, recursive_var : forall x1:A1, ..., xn:An, B, x1:A1, ..., xn:An |- body : T
// ------------------------------------------------
//    gamma |- fix recursive_var (arg1: A1) (arg2: A2) ... (argn: An) {arg_decreasing} := body : T
// Where gamma is the context which recursive_var is well-typed in.
//
// Additionally: all recursive calls recursive_var u1 ... un satisfy: u_decreasing_arg_index is
// structurally smaller than args[decreasing_arg_index].
Expression *init_fix_expression_wc(Expression *recursive_var, Expression **args, int arg_count,
                                   int decreasing_arg_index, Expression *body);

// Returns the uplinks of an expression.
DoublyLinkedList *get_expression_uplinks(Expression *expression);

// Returns the type of an expression.
Expression *get_expression_type(Expression *expression);

// Returns the body of an expression, or null if the variable is opaque.
Expression *get_expression_body(Expression *expression);

// Returns the context of an expression.
Context *get_expression_context(Expression *expression);

// Returns the innermost body of an expression. For example, if the expression
// is
//     fun x: A => fun y: B => C
// then get_innermost_body(expression) will return C.
Expression *get_innermost_body(Expression *expression);

// Returns the innermost function of an expression. For example, if the
// expression is
//     (((f x3) x1) x0)
// then get_innermost_func(expression) will return f.
Expression *get_innermost_func(Expression *expression);

// Returns the name of a variable expression.
char *get_var_name(Expression *expr);

// Returns the name of a hole expression.
char *get_hole_name(Expression *expr);

// Returns the function of an application.
Expression *get_app_func(Expression *expr);

// Returns the argument of an application.
Expression *get_app_arg(Expression *expr);

Expression *get_forall_bound_variable(Expression *expr);

Expression *get_lambda_bound_variable(Expression *expr);

// Returns the arity of an expression.
int get_arity(Expression *expr);

Expression *get_forall_body(Expression *expr);

Expression *get_lambda_body(Expression *expr);

Expression *get_arrow_lhs(Expression *expr);

Expression *get_arrow_rhs(Expression *expr);

// Returns the value of the maybe_hole_free field of an expression.
// This is a heuristic to determine if the expression may contain holes.
// The reason this exists is because an expression containing a hole may be
// modified to remove the hole, via a call to `fill_hole(hole, term)`. This
// heuristic can be removed in the future if we implement proper upwards
// traversal of the expression tree to update the maybe_hole_free field.
bool get_maybe_hole_free(Expression *expr);

// Returns true if the expression has any holes.
bool has_holes(Expression *expr);

// Returns true if the expression is a hole.
bool is_hole(Expression *expr);

// Returns true if the term can fill the hole.
bool can_fill(Expression *hole, Expression *term);

// Fills a hole with a term. The term must satisfy the following conditions:
//    1) The type of term  expected return type of hole.
//    2) The defining context of hole contains the context(term).
//    3) Term does not itself contain the hole.
// This does no modifications/creates no new objects. Instead, it modifies the
// uplinks of the hole to point to the term.
void fill_hole(Expression *hole, Expression *term);

// Returns true if var_or_hole appears as a subterm in term
bool occurs_in(Expression *var_or_hole, Expression *term);

// Returns true if the expressions are alpha-congruent.
bool congruence(Expression *a, Expression *b);

// Returns true if the expressions are alpha-congruent while allowing holes.
// When a hole is encountered, it checks if the other expression can fill it.
// This is useful for type checking when holes are present in the expressions.
bool congruent_with_holes(Expression *a, Expression *b);

// Returns true if a is a subtype of b. We don't implement a full subtyping
// relation, but it is necessary specifically for Type and Prop.
bool subtypes(Expression *a, Expression *b);

// This function compares a and b, creating a mapping of variables in a to
// variables in b. Specifically, assuming a and b are alpha-congruent, it
// creates a mapping of the bound variables in a to the bound variables in b. It
// then substitutes the variables in to_subst with the mapping creating [a ->
// b].
Expression *match_and_subst(Expression *a, Expression *b, Expression *to_subst);
bool congruence2(Expression *a, Expression *b);

extern char c_counter;
char *get_char();

#endif  // EXPRESSION_H/