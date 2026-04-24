#ifndef ORDER_H
#define ORDER_H

#include <stdbool.h>

#include "src/kernel/expression.h"

/*
 * Order Data Structure interface. Maintains a partial order over variables.
 *
 *   order_on_insert(parent, new_var)
 *       Called immediately after a new VAR_EXPRESSION `new_var` is created
 *       with parent context `parent`.  Corresponds to Insert(parent, new_var).
 *
 *   order_on_delete(var)
 *       Called when a VAR_EXPRESSION is freed.  Corresponds to Delete(var).
 *
 *   order_precedes(x, y)
 *       Returns true iff x is an ancestor of y.
 */

/* Notify the order structure that new_var was inserted after parent. */
void order_on_insert(Expression *parent, Expression *new_var);

/* Notify the order structure that var has been deleted. */
void order_on_delete(Expression *var);

/*
 * Returns true iff x precedes y in the order (i.e. x is an ancestor-or-equal
 * of y in the context chain).
 */
bool order_precedes(Expression *x, Expression *y);

#endif  // ORDER_H
