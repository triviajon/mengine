#include <stdio.h>
#include <stdlib.h>
#include "context.h"
#include "expression.h"
#include "utils.h"

int main() {
    // Example use-case
    Context *empty = context_create_empty();

    Expression *x = init_var_expression("x");
    Expression *f = init_var_expression("f");
    Expression *Type = init_type_expression();
    Expression *blank_var = init_var_expression("_");

    Context *f_type_context = context_insert(empty, blank_var, Type); // Defines the context [_: Type]*
    Expression *Type_to_Type = init_forall_expression(f_type_context, Type); // forall [_: Type], Type. i.e., Type -> Type.

    Context *c = context_insert(context_insert(empty, f, Type_to_Type), x, Type); // Builds the context [x: Type][f: Type -> Type]*

    Expression *f_x = init_app_expression(c, f, x); // Builds the application express (f x)
    Expression *lambda = init_lambda_expression(c, f_x); // Builds the lambda term "fun (c) -> f_x" which expands to "fun [x: Type][f: Type -> Type] -> f x" 

    printf("Term: %s\n", stringify_expression(lambda));

    printf("Term's type: %s\n", stringify_expression(get_expression_type(c, lambda)));

    printf("Successfully ran program. \n");

    return 0;
}
