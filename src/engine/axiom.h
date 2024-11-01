#ifndef AXIOM_H
#define AXIOM_H

#include "expression.h"
#include "context.h"

extern Expression *eq;
extern Expression *eq_refl;
extern Expression *f_equal;
extern Expression *eq_trans;
extern Expression *f;
extern Expression *g;
extern Expression *a;
extern Expression *eq_fa_a;
extern Expression *nat;
extern Context *f_a_ctx; 
extern Context *g_f_a_ctx; 


void init_globals();
Expression *build_fa();
bool equivalent_under_computation(Expression *a, Expression *b);

#endif // AXIOM_H
