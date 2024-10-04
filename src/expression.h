#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "doubly_linked_list.h"
#include "context.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Forward declaration of Expression
typedef struct Expression Expression;

typedef enum {
  VAR_EXPRESSION,
  LAMBDA_EXPRESSION,
  APP_EXPRESSION,
  FORALL_EXPRESSION,
  TYPE_EXPRESSION,
  HOLE_EXPRESSION
} ExpressionType;

/*
An uplink is a combination of
    1) a pointer to a expression (one of my parents), and
    2) an uplink relation (how what am I to that parent)
*/
typedef enum { LAMBDA_BODY, APP_FUNC, APP_ARG } Relation;
typedef struct {
  Expression *expression;
  Relation relation;
} Uplink;

/*
A VarExpression is a combination of
    1) a string name, and
    2) a list of uplinks.

VarExpression instances are responsibile for allocating and freeing the memory
used to store the doubly-linked list of uplinks, but not the Uplinks within
each DLLExpression. Each DLLExpression contains a pointer to an Uplink, which
should be freed by the Expression it pointers to.
*/
typedef struct {
  char *name;
  DoublyLinkedList *uplinks;
} VarExpression;

// A Lambda expression. The body provided must be valid under the given context. 3
typedef struct {
  Context *context; 
  Expression *type;
  Expression *body;
  DoublyLinkedList *uplinks;
} LambdaExpression;
/*
Similar to VarExpression, but we also include an optional cache.
*/
typedef struct {
  Expression *func;
  Expression *arg;
  Expression *cache; // Possibly NULL
  Expression *type; // Type of the app expression, which should be the type of func with it's binding variable substituted by arg
  Context *context;  
  DoublyLinkedList *uplinks;
} AppExpression;

/*
Represented the same way as a LambdaExpression.
*/
typedef struct {
  Context *context; 
  Expression *type;
  Expression *body;
  DoublyLinkedList *uplinks;
} ForallExpression;

typedef struct {
  DoublyLinkedList *uplinks;
} TypeExpression;

typedef struct {
  char *name; 
  Expression *type; // The intended return type of the hole
  Context *context;
  DoublyLinkedList *uplinks;
} HoleExpression;

struct Expression {
  ExpressionType type;
  union {
    VarExpression var;
    LambdaExpression lambda;
    AppExpression app;
    ForallExpression forall;
    TypeExpression type;
    HoleExpression hole;
  } value;
};

Expression *init_var_expression(const char *name);
Expression *init_lambda_expression(Context *context, Expression *body);
Expression *init_app_expression(Context *context, Expression *func, Expression *arg);
Expression *init_forall_expression(Context *context, Expression *body);
Expression *init_type_expression();
Expression *init_hole_expression(char *name, Expression *type, Context *context);

Expression *get_expression_type(Context *context, Expression *expression);

void free_expression(Expression *expr);

#endif // EXPRESSION_H