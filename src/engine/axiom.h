#ifndef AXIOM_H
#define AXIOM_H

#include "expression.h"
#include "context.h"
#include "dyn_array_map.h"

extern Expression *eq;
extern Expression *eq_refl;
extern Expression *app_cong;
extern Expression *eq_trans;
extern Expression *lambda_extensionality;
extern Expression *f;
extern Expression *g;
extern Expression *h;
extern Expression *a;
extern Expression *b;
extern Expression *c;
extern Expression *eq_fa_a;
extern Expression *eq_hxx_x;
extern Expression *nat;
extern Expression *add;
extern Expression *add_r_O;
extern Expression *O;


void init_globals();
bool congruence(Expression *a, Expression *b);

#endif // AXIOM_H
