#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "doubly_linked_list.h"

// Forward declaration of Expression & Context
typedef struct Expression Expression;
typedef struct Context Context;

typedef enum {
  VAR_EXPRESSION, 
  LAMBDA_EXPRESSION,
  APP_EXPRESSION,
  FORALL_EXPRESSION,
  TYPE_EXPRESSION,
  PROP_EXPRESSION,
  HOLE_EXPRESSION
} ExpressionType;

/*
An uplink is a combination of
    1) a pointer to a expression (one of my parents), and
    2) an uplink relation (how what am I to that parent)
*/
typedef enum { LAMBDA_BODY, APP_FUNC, APP_ARG, FORALL_BODY } Relation;
typedef struct {
  Expression *expression;
  Relation relation;
} Uplink;

typedef struct {
  char *name;
  Expression *type;
  DoublyLinkedList *uplinks;
} VarExpression;

typedef struct {
  Context *context;
  Expression *bound_variable;
  Expression *type;
  Expression *body;
  DoublyLinkedList *uplinks;
} LambdaExpression;

typedef struct {
  Expression *func;
  Expression *arg;
  Expression *cache;
  Expression *type; 
  Context *context;
  DoublyLinkedList *uplinks;
} AppExpression;

typedef struct {
  Context *context;
  Expression *bound_variable;
  Expression *type;
  Expression *body;
  DoublyLinkedList *uplinks;
} ForallExpression;

typedef struct {
  DoublyLinkedList *uplinks;
} TypeExpression;

typedef struct {
  DoublyLinkedList *uplinks;
} PropExpression;

static Expression *TYPE = NULL;
static Expression *PROP = NULL;

typedef struct {
  char *name;
  Expression *return_type;
  Context *defining_context;
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
    PropExpression prop;
    HoleExpression hole;
  } value;
};

void add_to_parents(Expression *expression, Uplink *uplink);
Uplink *new_uplink(Expression *parent, Relation relation);

Expression *init_var_expression(const char *name, Expression *type);
Expression *init_lambda_expression(Expression *bound_variable, Expression *body);
Expression *init_app_expression(Expression *func, Expression *arg);
Expression *init_forall_expression(Expression *bound_variable, Expression *body);
Expression *init_type_expression();
Expression *init_prop_expression();
Expression *init_hole_expression(char *name, Expression *return_type, Context *defining_context);
Expression *init_arrow_expression(Expression *lhs, Expression *rhs);

DoublyLinkedList *get_expression_uplinks(Expression *expression);
Expression *get_expression_type(Expression *expression);
Context *get_expression_context(Expression *expression);

void fillHole(Expression *hole, Expression *term);

void free_expression(Expression *expr);

#endif  // EXPRESSION_H