#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "doubly_linked_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef enum {
  VAR_EXPRESSION,
  LAMBDA_EXPRESSION,
  APP_EXPRESSION,
  FORALL_EXPRESSION,
  TYPE_EXPRESSION
} ExpressionType;

typedef struct Expression Expression;

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

/*
Similar to VarExpression.
*/
typedef struct {
  VarExpression *var;
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
  DoublyLinkedList *uplinks;
} AppExpression;

/*
Similar to above.
*/
typedef struct {
  VarExpression *var;
  Expression *type;
  Expression *arg;
  DoublyLinkedList *uplinks;
} ForallExpression;

typedef struct {
  DoublyLinkedList *uplinks;
} TypeExpression;

struct Expression {
  ExpressionType type;
  union {
    VarExpression var;
    LambdaExpression lambda;
    AppExpression app;
    ForallExpression forall;
    TypeExpression type;
  } value;
};

typedef struct {
  char *name;
  Expression *theorem;
  Expression *proof;
} Theorem;

/*
A "program" is just an step (let statement, theorem statement, or expression), and
a pointer to the next step (or null)
*/
typedef enum { LET_STEP, THEOREM_STEP, EXPR_STEP } StepType;

typedef struct Step {
  StepType type;
  union {
    struct {
      char *id;
      Expression *expr;
    } let;

    struct {
      Theorem *theorem;
    } theorem;

    struct {
      Expression *expr;
    } expr;
  } value;

  struct Step *next;
} Step;

Step *init_let_step(char *id, Expression *expr, Step *next);
Step *init_theorem_step(Theorem *theorem, Step *next);
Step *init_expr_step(Expression *expr);

Expression *init_var_expression(const char *name);
Expression *init_lambda_expression(Expression *var, Expression *type, Expression *body);
Expression *init_app_expression(Expression *func, Expression *arg);
Expression *init_forall_expression(Expression *var, Expression *type, Expression *arg);
Expression *init_type_expression();

Expression *free_var_expression(Expression *expr);
Expression *free_lambda_expression(Expression *expr);
Expression *free_app_expression(Expression *expr);
Expression *free_forall_expression(Expression *expr);
Expression *free_type_expression(Expression *expr);

bool equal(Expression *a, Expression *b);
char *stringify_expression(Expression *expression);

Expression *set_in_context(Expression *context, Expression *var, Expression *expr);
Expression *lookup_in_context(Expression *context, Expression *var);

#endif // EXPRESSION_H