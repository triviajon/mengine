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
} ExpressionType;

// Represents a parent-child relationship between expressions
typedef enum {
    LAMBDA_BODY,
    APP_FUNC,
    APP_ARG,
    FORALL_BODY,
    CTX_VAR,
    HOLE_TYPE,
    VAR_BODY,
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
    char *name;  // User-friendly name for the variable. Not used internally.
    Expression *definition;  // If the variable is non-opaque, then this field
                             // will be non-NULL with the definition body
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

// Represents a generic expression.
struct Expression {
    ExpressionType tag;  // The kind of expression (VAR, LAMBDA, APP, etc.)

    // Common fields across most expression types
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced
    Context *context;      // The minimal context this expression is valid in
                           // NULL for TYPE and PROP
    int ctx_size;          // Size of the context (0 for empty, 1+ for variables)
                           // 0 for TYPE and PROP
    Expression *type;      // The type of this expression
                           // NULL for TYPE and PROP
    bool maybe_hole_free;  // True means term is hole-free, false means may
                           // contain holes. Unused for TYPE and PROP.

    union {
        VarExpression var;
        LambdaExpression lambda;
        AppExpression app;
        ForallExpression forall;
        TypeExpression type_expr;
        PropExpression prop_expr;
        HoleExpression hole;
    } as;
};

// Helper function to add an uplink to the uplinks list of an expression.
void add_to_parents(Expression *expression, void *ptr, Relation r);

// Helper function to remove the first top_level_hole uplink from an
// expression's uplinks.
void remove_tl_uplink(Expression *expression);

// Create a new uplink describing how ptr relates.
Uplink *new_uplink(void *ptr, Relation r);

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
Expression *init_hole_expression(char *name, Expression *return_type,
                                 Context *gamma);

// Create a new variable expression with a given name, type, and defining
// context. Typing rule:
//    gamma |- type : s
//    s in {Prop, Type_i}
// ------------------------------------------------
//    gamma, name : type |-
// In other words, if the type is valid in the input context, and the type(s) is
// a Prop/Type_i, then this variable is valid in the extension of the context.
Expression *init_var_expression_wc(const char *name, Expression *type,
                                   Context *gamma);

// Create a new variable expression with a given name, definition, and context.
// context. Typing rule:
//    gamma |- definition : A
//    gamma |- A : a
//    s in {Prop, Type_i}
// ------------------------------------------------
//    gamma, x : A |-
// In other words, if the type is valid in the input context, and the type(s) is
// a Prop/Type_i, then this variable is valid in the extension of the context.
Expression *init_var_expression_wc_with_definition(const char *name,
                                                   Expression *definition,
                                                   Context *gamma);

// Create a new lambda/abstraction expression with a bound variable and body.
// Typing rule:
//     gamma, bound_variable : A |- body : B
// ------------------------------------------------
//     gamma |- fun (bound_variable: A) => body : Forall (bound_variable: A), B
// Where gamma = context(bound_variable).
Expression *init_lambda_expression_wc(Expression *bound_variable,
                                      Expression *body);

// Create a new application expression with a function, argument, and context.
// Typing rule:
//    gamma |- func : Forall bound_variable: A, B
//    gamma |- arg : A
// ------------------------------------------------
//    gamma |- func arg : B[bound_variable -> arg]
// The context parameter should typically be the longer of context(func) and context(arg).
Expression *init_app_expression_wc(Expression *func, Expression *arg,
                                   Context *context);

// Create a new forall expression with a bound variable and body.
// Typing rule:
//    gamma, bound_variable : A |- body : s, s in {Prop, Type_i}
//    (if s = Type_i, then) gamma |- A : s
// ------------------------------------------------
//    gamma |- Forall bound_variable: A, body : s
// Where gamma = context(bound_variable).
Expression *init_forall_expression_wc(Expression *bound_variable,
                                      Expression *body);

// Create a new arrow expression with a left-hand side, right-hand side, and context.
// The typing rule is a special case of the forall expression typing rule.
// Typing rule:
//    gamma, _ : lhs |- rhs : s, s in {Prop, Type_i}
//    (if s = Type_i, then) gamma |- lhs : s
// ------------------------------------------------
//    gamma |- Forall _: lhs, rhs : s (which is equivalent to "lhs -> rhs")
// The context parameter should typically be the longer of context(lhs) and context(rhs).
Expression *init_arrow_expression_wc(Expression *lhs, Expression *rhs,
                                     Context *gamma);

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

// Frees an expression and all its children.
void free_expression(Expression *expr);

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