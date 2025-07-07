#ifndef REWRITE_NM_H
#define REWRITE_NM_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

extern Expression *f_nm;
extern Expression *x0_nm;
extern Expression *f_n_x0_nm;

RewriteProof *rewrite_nm(int n, int m);

#endif // REWRITE_NM_H