#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/engine/rewrite_proof.h"
#include "src/kernel/doubly_linked_list.h"

// Forward declarations
typedef struct Expression Expression;
typedef struct Context Context;

// Supported expression types for the Expression struct.
typedef enum {
    VAR_EXPRESSION,
    LAMBDA_EXPRESSION,
    APP_EXPRESSION,
    FORALL_EXPRESSION,
    TYPE_EXPRESSION,
    PROP_EXPRESSION,
    FIX_EXPRESSION,
    HOLE_EXPRESSION,
    MATCH_EXPR_EXPRESSION,
} ExpressionType;

// Represents a parent-child relationship between expressions
typedef enum {
    LAMBDA_BODY,
    APP_FUNC,
    APP_ARG,
    FORALL_BODY,
    CTX_VAR,
    HOLE_TYPE,
    TOP_LEVEL_HOLE,
} Relation;

/*
An uplink is a combination of
    1a) a pointer to a expression (one of my parents) OR
    1b) a pointer to a context (where I am referenced)
    2) an uplink relation (how what am I to that parent)

    Either 1a or 1b will ever be true, but never both.
*/
typedef struct {
    Expression *expression;
    Context *context;
    Relation relation;
} Uplink;

// A variable/type binding.
typedef struct {
    char *name;  // User-friendly name for the variable. Not used internally.
    Expression *type;  // The type of the variable.
    Context *context;  // The minimal context which this expression is valid in.
                       // In this case, it is the context which is needed to
                       // define the type of the variable.
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
    RewriteProof *rresult;  // When rewriting, cache of the result. NULL while
                            // not rewriting.
    bool maybe_hole_free;   // A value of true means that the term is hole-free,
                            // false means that it may contain holes.
} VarExpression;

// A lambda expression: fun (bound_variable) => body.
typedef struct {
    Context *context;  // The minimal context which this expression is valid in.
                       // In this case, it is the context which the body is
                       // valid in minus the bound variable.
    Expression *bound_variable;  // The bound variable of the lambda.
    Expression *type;  // The type of the lambda expression, which is a Forall
                       // with similar structure.
    Expression *body;  // The body of the lamdbda expression.
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
    RewriteProof *rresult;  // When rewriting, cache of the result. NULL while
                            // not rewriting.
    bool maybe_hole_free;   // A value of true means that the term is hole-free,
                            // false means that it may contain holes.
} LambdaExpression;

// An application expression: (func arg).
typedef struct {
    Expression *func;   // The function which is applied to the argument. Has
                        // type Forall...
    Expression *arg;    // The argument being operating on.
    Expression *cache;  // A copied version of this application which is used in
                        // beta-reduction with Lambda-DAGs
    Expression *type;   // The type of this application expression.
    Context *context;  // The minimal context which this expression is valid in.
                       // In this case, it is the context which both the func
                       // and the arg are valid in.
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
    RewriteProof *rresult;  // When rewriting, cache of the result. NULL while
                            // not rewriting.
    bool maybe_hole_free;   // A value of true means that the term is hole-free,
                            // false means that it may contain holes.
} AppExpression;

// Similar to LambdaExpression.
typedef struct {
    Context *context;
    Expression *bound_variable;
    Expression *type;  // Always a "Type" expression
    Expression *body;
    DoublyLinkedList *uplinks;
    bool maybe_hole_free;  // A value of true means that the term is hole-free,
                           // false means that it may contain holes.
} ForallExpression;

typedef struct {
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
} TypeExpression;

typedef struct {
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
} PropExpression;

// TYPE and PROP are the only structs with the TypeExpression and PropExpression
// ExpressionTypes, i.e., they are singletons.
static Expression *TYPE = NULL;
static Expression *PROP = NULL;

// A typed hole to be filled later.
typedef struct {
    char *name;  // A user-friendly name for the hole. Not used internally.
    Expression *return_type;    // The required type for the hole.
    Context *defining_context;  // The context which this hole was defined in.
    DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
    bool maybe_hole_free;  // A value of true means that the term is hole-free,
                           // false means that it may contain holes.
} HoleExpression;

// A fix expression: fix ident (bound_variable) => body.
typedef struct {
    Expression *ident;
    Expression *bound_variable;
    Expression *body;
    Context *context;
    Expression *type;
    DoublyLinkedList *uplinks;
    bool maybe_hole_free;  // A value of true means that the term is hole-free,
                           // false means that it may contain holes.
} FixExpression;

// Special case; we don't implement matching in general for this POC.
typedef struct {
    Expression *match_scrutinee;
    Expression *literal_case_item;
    Expression *literal_result;
    Expression *var_case_item;
    Expression *var_result;
    Expression *op_case_item;
    Expression *op_result;
    Context *context;
    Expression *type;
    DoublyLinkedList *uplinks;
    bool maybe_hole_free;  // A value of true means that the term is hole-free,
                           // false means that it may contain holes.
} MatchExprExpression;

// Represents a generic expression.
struct Expression {
    ExpressionType type;
    union {
        VarExpression var;
        LambdaExpression lambda;
        AppExpression app;
        ForallExpression forall;
        TypeExpression type;
        PropExpression prop;
        HoleExpression hole;
        FixExpression fix;
        MatchExprExpression matchExpr;
    } value;
};

// Helper function to add an uplink to the uplinks list of an expression.
void add_to_parents(Expression *expression, Uplink *uplink);

// Helper function to remove the first top_level_hole uplink from an
// expression's uplinks.
void remove_tl_uplink(Expression *expression);

// Helper function to create a new uplink to an expression.
Uplink *new_uplink(Expression *parent, Relation relation);

// Helper function to create a new uplink to a context.
Uplink *new_uplink2(Context *parent, Relation relation);

// Helper function to create a new uplink to a top level hole.
Uplink *new_uplink_tl();

// The following functions are helper functions to initialize new expressions,
// without providing explicit contexts. The function will take care of computing
// the context needed to type the expression. For example:
//  init_app_expression(func, arg) will take the contexts of func and arg, and
//  "merge" them into a single context, and then use that context to type the
//  expression, which necessarily
//   types the expression given that the inputs are valid in their respective
//   contexts.
// These are useful convenience functions, but are not always the best choice.

Expression *init_var_expression(const char *name, Expression *type);
Expression *init_lambda_expression(Expression *bound_variable,
                                   Expression *body);
Expression *init_app_expression(Expression *func, Expression *arg);
Expression *init_forall_expression(Expression *bound_variable,
                                   Expression *body);
Expression *init_type_expression();
Expression *init_prop_expression();
Expression *init_fix_expression(Expression *ident, Expression *bound_variable,
                                Expression *body);
Expression *init_match_expr_expression(Expression *match_scrutinee,
                                       Expression *literal_scrutinee,
                                       Expression *literal_result,
                                       Expression *var_scrutinee,
                                       Expression *var_result,
                                       Expression *op_scrutinee,
                                       Expression *op_result, Expression *type);

// Initialize a new hole expression with a given name, return type, and defining
// context. To fill the hole using `fill_hole(hole, term)`, the term used to
// fill the hole must be valid in the hole's defining context.
Expression *init_hole_expression(char *name, Expression *return_type,
                                 Context *defining_context);

// The following functions are helper functions to initialize new expressions,
// with explicit contexts. These are useful when you have a specific context in
// mind for the expression, and you want to use that context to type the
// expression.

// Initialize a new variable expression with a given name, type, and defining
// context. The variable's type must be valid in the defining context.
Expression *init_var_expression_wc(const char *name, Expression *type,
                                   Context *defining_context);

// Initialize a new lambda expression with a bound variable, body, and context.
// The body must be valid in the given context.
Expression *init_lambda_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *context);

// Initialize a new application expression with a function, argument, and
// context. The function and argument must be valid in the given context.
Expression *init_app_expression_wc(Expression *func, Expression *arg,
                                   Context *context);

// Initialize a new forall expression with a bound variable, body, and context.
// The body must be valid in the given context.
Expression *init_forall_expression_wc(Expression *bound_variable,
                                      Expression *body, Context *context);

// Initializes a new "arrow" expression.
// The "arrow" expression is a shorthand for a forall expression with a bound
// variable. init_arrow_expression(A, B) is equivalent to
// init_forall_expression(init_var_expression("_", A), B).
Expression *init_arrow_expression(Expression *lhs, Expression *rhs);

// Returns the uplinks of an expression.
DoublyLinkedList *get_expression_uplinks(Expression *expression);

// Returns the type of an expression.
Expression *get_expression_type(Expression *expression);

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

// Take a lambda expression as input, like fun x: T => B, and
// return a new lambda expression with a fresh variable x' and
// x substituted for by x'.
Expression *refresh(Expression *expr);

#endif  // EXPRESSION_H/