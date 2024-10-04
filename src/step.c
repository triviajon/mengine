#include "step.h"

Step *init_let_step(char *id, Expression *expr, Step *next) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = LET_STEP;
    step->value.let.id = id;
    step->value.let.expr = expr;
    step->next = next;
    return step;
}

Step *init_theorem_step(Theorem *theorem, Step *next) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = THEOREM_STEP;
    step->value.theorem.theorem = theorem;
    step->next = next;
    return step;
}

Step *init_expr_step(Expression *expr) {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = EXPR_STEP;
    step->value.expr.expr = expr;
    step->next = NULL;
    return step;
}

Step *init_end_step() {
    Step *step = (Step*)malloc(sizeof(Step));
    step->type = END_STEP;
    step->next = NULL;
    return step;
}
