#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/order.h"

/*
 * Linked-list implementation of the Order Data Structure.
 *  Insert - no-op: the context field set at construction encodes position
 *  Delete - no-op: freed by the GC with no extra bookkeeping required
 *  Order  - O(size(y)): walk from y toward the root checking if x is found.
 */

#ifdef ORDER_USE_LL

void order_on_insert(Expression *parent, Expression *new_var) {
    (void)parent;
    (void)new_var;
}

void order_on_delete(Expression *var) { (void)var; }

bool order_precedes(Expression *x, Expression *y) {
    Expression *curr = y;
    while (!context_is_empty(curr)) {
        if (curr == x) {
            return true;
        }
        curr = get_expression_context(curr);
    }
    return false;
}

#endif  // ORDER_USE_LL
