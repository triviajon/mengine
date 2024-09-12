#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "doubly_linked_list.h"
#include <stddef.h>
#include <stdbool.h>


typedef enum { VAR_EXPRESSION, LAMBDA_EXPRESSION, APP_EXPRESSION } ExpressionType;


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
each DLLExpression. Each DLLExpression contains a pointer to an Uplink, which should be
freed by the Expression it pointers to.
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

typedef struct {
  ExpressionType type;
  union Expression {
    VarExpression var;
    LambdaExpression lambda;
    AppExpression app;
  } value;
} Expression;

char* stringify_expression(Expression* expression);

Expression* create_var_expression(const char* name);
Expression* create_lambda_expression(Expression* var, Expression* body);
Expression* create_app_expression(Expression* func, Expression* arg);

int replace_child(DoublyLinkedList* old_parents, Expression* new_child);
Expression* scandown(Expression* expression, Expression* argterm, DoublyLinkedList* varpars, Expression** topapp);
Expression* beta_reduction(Expression* expression);
int upcopy(Expression* new_child, Expression* parent, Relation relation);
int clear_caches(Expression* reduced_lambda_expression, Expression* top_app_expression);
int free_dead_expression(Expression* expression);

#endif // EXPRESSION_H