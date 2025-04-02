#ifndef REWRITE_SINGLE_ARGUMENT_H
#define REWRITE_SINGLE_ARGUMENT_H

#include <stdio.h>
#include <stdlib.h>

#include "axiom.h"
#include "tactics.h"
#include "context.h"
#include "expression.h"
#include "utils.h"

// Build (f ... (f a)) with `f_length` number of f's. If `g_wrap` is non-zero,
// wrap the resulting expression with `g` to form g(f(...(f(a)))). Attempts to
// rewrite using the lemma eq_fa_a.
RewriteProof *rewrite_gfa(int f_length, int g_wrap);

#endif // REWRITE_SINGLE_ARGUMENT_H