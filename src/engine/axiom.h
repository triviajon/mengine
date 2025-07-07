#ifndef AXIOM_H
#define AXIOM_H

#include "context.h"
#include "dyn_array_map.h"
#include "expression.h"

extern Expression *eq;
extern Expression *eq_refl;
extern Expression *eq_sym;
extern Expression *eq_subst;
extern Expression *and;
extern Expression *and_conj;
extern Expression * or ;
extern Expression *or_introl;
extern Expression *or_intror;
extern Expression *iff1;
extern Expression *iff1_refl;
extern Expression *iff1_sym;
extern Expression *iff1_trans;
extern Expression *iff1_subst;
extern Expression *iff1_cong;
extern Expression * not ;
extern Expression *app_cong;
extern Expression *ex;
extern Expression *ex_intro;
extern Expression *True;
extern Expression *I;
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

extern Expression *option;
extern Expression *option_some;
extern Expression *option_none;
extern Expression *partial_map;
extern Expression *partial_map_empty;
extern Expression *partial_map_get;
extern Expression *partial_map_put;
extern Expression *partial_map_remove;
extern Expression *partial_map_get_put_same;
extern Expression *partial_map_get_put_diff;
extern Expression *partial_map_put_put_same;

Expression *iff(Expression *A, Expression *B);
Expression *impl1(Expression *T, Expression *P, Expression *Q);
Expression *and1(Expression *T, Expression *P, Expression *Q);
Expression *or1(Expression *T, Expression *P, Expression *Q);
Expression *ex1(Expression *A, Expression *B, Expression *P);

extern Context *std_lib_ctx;

void init_globals();

#endif  // AXIOM_H
