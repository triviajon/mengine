#ifndef REWRITE_NM_H
#define REWRITE_NM_H

#include <stdio.h>
#include <stdlib.h>

#include "src/engine/axiom.h"
#include "src/engine/tactics.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"

extern Expression *f_nm;
extern Expression *x0_nm;
extern Expression *f_n_x0_nm;

RewriteProof *rewrite_nm(int n, int m);

#endif  // REWRITE_NM_H