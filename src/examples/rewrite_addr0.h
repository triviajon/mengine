#ifndef REWRITE_ADDR0
#define REWRITE_ADDR0

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

RewriteProof *rewrite_addr0__letin(int n_depth);

RewriteProof *rewrite_addr0__tree(int n_depth);

RewriteProof *rewrite_addr0__native(int n_depth);

#endif  // REWRITE_ADDR0