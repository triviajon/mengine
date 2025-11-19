#ifndef REWRITE_OPEN_HOLES_H
#define REWRITE_OPEN_HOLES_H

#include <stdio.h>
#include <stdlib.h>

#include "src/engine/axiom.h"
#include "src/engine/tactics.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"

RewriteProof *rewrite_open_holes();

#endif  // REWRITE_OPEN_HOLES_H