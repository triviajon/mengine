#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include "src/engine/axiom.h"
#include "src/engine/tactics.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"

void print_rwpf__coq_ready(RewriteProof *rw_pf, int withlet_flag,
                           int debug_flag);

void print_rwpf__no_proof(RewriteProof *rw_pf, int withlet_flag,
                          int debug_flag);

#endif  // PRINT_UTILS_H