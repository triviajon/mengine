#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doubly_linked_list.h"
#include "rewrite_proof.h"

// Forward declarations
typedef struct Expression Expression;
typedef struct Context Context;

// Supported expression types for the Expression struct.
typedef enum
{
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
typedef enum
{
  LAMBDA_BODY,
  APP_FUNC,
  APP_ARG,
  FORALL_BODY,
  CTX_VAR,
  HOLE_TYPE
} Relation;

/*
An uplink is a combination of
    1a) a pointer to a expression (one of my parents) OR
    1b) a pointer to a context (where I am referenced)
    2) an uplink relation (how what am I to that parent)

    Either 1a or 1b will ever be true, but never both.
*/
typedef struct
{
  Expression *expression;
  Context *context;
  Relation relation;
} Uplink;

// A variable/type binding.
typedef struct
{
  char *name;                // User-friendly name for the variable. Not used internally.
  Expression *type;          // The type of the variable.
  Context *context;          // The minimal context which this expression is valid in.
                             // In this case, it is the context which is needed to
                             // define the type of the variable.
  DoublyLinkedList *uplinks; // Uplinks where this expression is referenced.
  RewriteProof *rresult;     // When rewriting, cache of the result. NULL while not rewriting.
} VarExpression;

// A lambda expression: fun (bound_variable) => body.
typedef struct
{
  Context *context;           // The minimal context which this expression is valid in.
                              // In this case, it is the context which the body is valid
                              // in minus the bound variable.
  Expression *bound_variable; // The bound variable of the lambda.
  Expression *type;           // The type of the lambda expression, which is a Forall
                              // with similar structure.
  Expression *body;           // The body of the lamdbda expression.
  DoublyLinkedList *uplinks;  // Uplinks where this expression is referenced.
  RewriteProof *rresult;      // When rewriting, cache of the result. NULL while not rewriting.
} LambdaExpression;

// An application expression: (func arg).
typedef struct
{
  Expression *func;          // The function which is applied to the argument. Has type
                             // Forall...
  Expression *arg;           // The argument being operating on.
  Expression *cache;         // A copied version of this application which is used in
                             // beta-reduction with Lambda-DAGs
  Expression *type;          // The type of this application expression.
  Context *context;          // The minimal context which this expression is valid in.
                             // In this case, it is the context which both the func and
                             // the arg are valid in.
  DoublyLinkedList *uplinks; // Uplinks where this expression is referenced.
  RewriteProof *rresult;     // When rewriting, cache of the result. NULL while not rewriting.
} AppExpression;

// Similar to LambdaExpression.
typedef struct
{
  Context *context;
  Expression *bound_variable;
  Expression *type; // Always a "Type" expression
  Expression *body;
  DoublyLinkedList *uplinks;
} ForallExpression;

typedef struct
{
  DoublyLinkedList *uplinks; // Uplinks where this expression is referenced.
} TypeExpression;

typedef struct
{
  DoublyLinkedList *uplinks; // Uplinks where this expression is referenced.
} PropExpression;

// TYPE and PROP are the only structs with the TypeExpression and PropExpression
// ExpressionTypes, i.e., they are singletons.
static Expression *TYPE = NULL;
static Expression *PROP = NULL;

// A typed hole to be filled later.
typedef struct
{
  char *name;                // A user-friendly name for the hole. Not used internally.
  Expression *return_type;   // The required type for the hole.
  Context *defining_context; // The context which this hole was defined in.
  DoublyLinkedList *uplinks; // Uplinks where this expression is referenced.
} HoleExpression;

// A fix expression: fix ident (bound_variable) => body.
typedef struct
{
  Expression *ident;
  Expression *bound_variable;
  Expression *body;
  Context *context;
  Expression *type;
  DoublyLinkedList *uplinks;
} FixExpression;

// Special case.
typedef struct
{
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
} MatchExprExpression;

// Represents a generic expression.
struct Expression
{
  ExpressionType type;
  union
  {
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

void add_to_parents(Expression *expression, Uplink *uplink);
Uplink *new_uplink(Expression *parent, Relation relation);
Uplink *new_uplink2(Context *parent, Relation relation);

Expression *init_var_expression(const char *name, Expression *type);
Expression *init_lambda_expression(Expression *bound_variable, Expression *body);
Expression *init_app_expression(Expression *func, Expression *arg);
Expression *init_forall_expression(Expression *bound_variable, Expression *body);
Expression *init_type_expression();
Expression *init_prop_expression();
Expression *init_hole_expression(char *name, Expression *return_type, Context *defining_context);
Expression *init_fix_expression(Expression *ident, Expression *bound_variable, Expression *body);
Expression *init_match_expr_expression(Expression *match_scrutinee, Expression *literal_scrutinee, Expression *literal_result, Expression *var_scrutinee, Expression *var_result,
                                       Expression *op_scrutinee, Expression *op_result, Expression *type);
Expression *init_arrow_expression(Expression *lhs, Expression *rhs);

DoublyLinkedList *get_expression_uplinks(Expression *expression);
Expression *get_expression_type(Expression *expression);
Context *get_expression_context(Expression *expression);
Expression *get_innermost_func(Expression *expression);

bool can_fill(Expression *hole, Expression *term);
void fillHole(Expression *hole, Expression *term);

void free_expression(Expression *expr);

bool congruence(Expression *a, Expression *b);
Expression *match_and_subst(Expression *a, Expression *b, Expression *to_subst);
void match_holes(Expression *a, Expression *b);
bool congruence2(Expression *a, Expression *b);

extern char c_counter;
char *get_char();

// Take a lambda expression as input, like fun x: T => B, and
// return a new lambda expression with a fresh variable x' and
// x substituted for by x'.
Expression *refresh(Expression *expr);

#endif // EXPRESSION_H/