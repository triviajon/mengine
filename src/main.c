#include <stdio.h>
#include <stdlib.h>
#include "typecheck.h"


int main() {

    Expression *P = init_var_expression("P");
    Expression *x = init_var_expression("x");
    Expression *type = init_type_expression();

    Expression *inner_forall = init_forall_expression(x, P, P);
    Expression *outer_forall = init_forall_expression(P, type, inner_forall); 

    Expression *inner_lambda = init_lambda_expression(x, P, x);
    Expression *outer_lambda = init_lambda_expression(P, type, inner_lambda);

    // Theorem p_implies_p : forall P: Type, forall x: P, P.
    // Proof. exact \P: Type, \x: P, x.  
    Theorem *p_implies_p = init_theorem("p_implies_p", outer_forall, outer_lambda);
    Step *theorem_definition = init_theorem_step(p_implies_p, init_end_step());

    printf("Setup is complete.\n");

    // Typecheck the context
    typecheck_prog(theorem_definition);

    printf("Successfully ran program. \n");

    return 0;
}
