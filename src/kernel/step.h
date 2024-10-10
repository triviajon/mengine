#include "expression.h"
#include "theorem.h"

/*
A "program" is just an step (let statement, theorem statement, or expression), and
a pointer to the next step (or null)
*/
typedef enum { LET_STEP, THEOREM_STEP, EXPR_STEP, END_STEP } StepType;

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
Step *init_end_step();