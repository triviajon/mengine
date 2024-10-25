#include "expression.h"

// Expression *beta_reduction(Expression *context, Expression *expression);

// int upcopy(Expression *new_child, Expression *parent, Relation relation);
// int free_dead_expression(Expression *expression);
int replace_child(DoublyLinkedList *old_parents, Expression *new_child);
// Expression *scandown(Expression *expression, Expression *argterm, DoublyLinkedList *varpars, Expression **topapp);
// int clear_caches(Expression *reduced_lambda_expression, Expression *top_app_expression);
// void clean_up(Expression *expression, Relation relation);