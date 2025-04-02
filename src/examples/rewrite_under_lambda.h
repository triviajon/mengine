#ifndef REWRITE_UNDER_LAMBDA_H
#define REWRITE_UNDER_LAMBDA_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"


RewriteProof *rewrite_lambda_f_x();
RewriteProof *rewrite_lambda_op_f_x();

#endif // REWRITE_UNDER_LAMBDA_H