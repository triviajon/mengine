#ifndef REWRITE_LET_IN_H
#define REWRITE_LET_IN_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"


// Build the expression in Jason Gross' PhD Thesis, figure 4-5. 
// Specifically builds it taking advantage of our system's
// ability to share. 
RewriteProof *rewrite_let_in__sharing(int n_depth);

#endif  // REWRITE_LET_IN_H