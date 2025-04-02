#ifndef REWRITE_CHAINED_MOD_H
#define REWRITE_CHAINED_MOD_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "context.h"
#include "expression.h"
#include "tactics.h"
#include "utils.h"

// Build the expression let x1 = a mod m in (let x2 = x1 mod m in ... in xn)
// by making use of our system's sharing. I.e., build (x1 mod m) mod m ...
// Then try to rewrite using the followin lemma.
// 
// mod_mod : forall a n : nat, (a mod n) mod n = a mod n.
RewriteProof *rewrite_chained_mod(int n_depth);

#endif // REWRITE_CHAINED_MOD_H