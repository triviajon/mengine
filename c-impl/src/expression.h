#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "doubly_linked_list.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  VAR_EXPRESSION,
  LAMBDA_EXPRESSION,
  APP_EXPRESSION,
  FORALL_EXPRESSION,
  TYPE_EXPRESSION
} ExpressionType;

typedef union Expression Expression;

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

typedef struct {
  ExpressionType type;
  union Expression {
    VarExpression var;
    LambdaExpression lambda;
    AppExpression app;
    ForallExpression forall;
    TypeExpression type;
  } value;
} Expression;

typedef struct {
  char *name;
  Expression *theorem;
  Expression *proof;
} Theorem;

/*
A program is just an step (let statement, theorem statement, or expression), and
a pointer to the next instruction (or null)
*/
typedef enum { LET_STEP, THEOREM_STEP, EXPR_STEP } StepType;

typedef struct {
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
} Program;

char *stringify_expression(Expression *expression);

Expression *init_var_expression(const char *name);
Expression *init_lambda_expression(Expression *var, Expression *body);
Expression *init_app_expression(Expression *func, Expression *arg);
Expression *init_forall_expression(Expression *var, Expression *type, Expression *arg);
Expression *init_type_expression();

Expression *free_var_expression(Expression *expr);
Expression *free_lambda_expression(Expression *expr);
Expression *free_app_expression(Expression *expr);
Expression *free_forall_expression(Expression *expr);
Expression *free_type_expression(Expression *expr);


int replace_child(DoublyLinkedList *old_parents, Expression *new_child);
Expression *scandown(Expression *expression, Expression *argterm,
                     DoublyLinkedList *varpars, Expression **topapp);
Expression *beta_reduction(Expression *expression);
int upcopy(Expression *new_child, Expression *parent, Relation relation);
int clear_caches(Expression *reduced_lambda_expression,
                 Expression *top_app_expression);
int free_dead_expression(Expression *expression);

#endif // EXPRESSION_H