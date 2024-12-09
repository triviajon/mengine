#ifndef REWRITE_MULTI_ARGUMENTS_H
#define REWRITE_MULTI_ARGUMENTS_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

// Build a multi-argument expression of depth h_depth. 
// h_depth = 1 -> (h a a)
// h_depth = 2 -> (h (h a a) (h a a))
// h_depth = 3 -> (h (h (h a a) (h a a)) (h (h a a) (h a a)))
// and so on, generally subsituting a -> (h a a) to get the next depth.
// Attempts to rewrite the resulting expression using the lemma eq_haa_a.
RewriteProof *rewrite_haa(int h_depth);


RewriteProof *rewrite_hxy();



#endif  // REWRITE_MULTI_ARGUMENTS_H