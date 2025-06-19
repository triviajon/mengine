#ifndef SEP_H
#define SEP_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

extern Expression *putmany;
extern Expression *disjoint;
extern Expression *split;

extern Expression *disjoint_comm;
extern Expression *disjoint_empty_l;
extern Expression *disjoint_empty_r;

extern Expression *putmany_comm;
extern Expression *putmany_empty_l;
extern Expression *putmany_empty_r;

extern Expression *emp;
extern Expression *sep;
extern Expression *ptsto;
extern Expression *read;

extern Expression *sep_cancel_r;

extern Expression *sep_comm;
extern Expression *sep_assoc;
extern Expression *sep_assoc_inv;
extern Expression *sep_neutral_r;
extern Expression *sep_neutral_l;
extern Expression *sep_assoc4;
extern Expression *sep_lift;
extern Expression *sep_cong;
extern Expression *sep_cancel;
extern Expression *sep_cancel_r_leaf;
extern Expression *sep_swap;

typedef struct {
  Expression *rewritten_expr;
  Expression *equality_proof;
} FlattenProof;

FlattenProof *init_flatten_proof(Expression *rewritten_expr, Expression *equality_proof);
FlattenProof *free_flatten_proof(FlattenProof *fp);

FlattenProof *associate_to_front(Expression *clause, Expression *v);

void run_sep1(int n);
void run_sep2(int n);

#endif // SEP_H