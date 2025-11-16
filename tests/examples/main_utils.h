#include "axiom.h"
#include "context.h"
#include "expression.h"
#include "tactics.h"
#include "utils.h"

void print_rwpf__coq_ready(RewriteProof *rw_pf, int withlet_flag,
                           int debug_flag);

void print_rwpf__no_proof(RewriteProof *rw_pf, int withlet_flag,
                          int debug_flag);