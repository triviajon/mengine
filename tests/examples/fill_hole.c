#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "expression.h"
#include "subst.h"

// This demonstrates that ?x + 1 cannot fill hole ?x.

void run_fill_hole(void) {
    Expression *nat = init_var_expression("nat", init_type_expression());

    Expression *hole =
        init_hole_expression("x", nat, get_expression_context(nat));

    Expression *one = init_var_expression("one", nat);
    Expression *add = init_var_expression(
        "add", init_arrow_expression(nat, init_arrow_expression(nat, nat)));

    Expression *app_add_x = init_app_expression(add, hole);
    Expression *app_add_x_one = init_app_expression(app_add_x, one);

    printf("Hole name: %s\n", hole->value.hole.name);
    printf("Attempting to fill hole with expression app_add_x_one\n");

    fill_hole(hole, app_add_x_one);

    if (has_holes(app_add_x_one)) {
        printf("Filling failed as expected: expression still has holes.\n");
    } else {
        printf("Filling unexpectedly succeeded: expression has no holes.\n");
    }
}
