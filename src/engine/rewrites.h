#ifndef REWRITES_H
#define REWRITES_H

#include "src/engine/axiom.h"
#include "src/engine/logic.h"
#include "src/engine/rewrite_proof.h"
#include "src/engine/rewrites.h"
#include "src/engine/unify.h"
#include "src/kernel/beta_reduction.h"
#include "src/kernel/context.h"
#include "src/kernel/utils.h"

RewriteProof *_rewrite(Context *goal_context, Expression *expr,
                       Expression *lemma);
RewriteProof *_rewrites(Context *goal_context, Expression *expr, int n,
                        va_list lemmas);
void clear_rewrite_proofs(Expression *expr);

RewriteProof *rewrite_head(Context *goal_context, Expression *expr,
                           Expression *lemma);
RewriteProof *rewrites_head(Context *goal_context, Expression *expr, int n,
                            va_list lemmas);
RewriteProof *rewrite(Context *goal_context, Expression *expr,
                      Expression *lemma);
RewriteProof *rewrites(Context *goal_context, Expression *expr, int n,
                       va_list lemmas);
RewrittenGoal *rewrite_transform(Expression *goal, Expression *rewrite_lemma);
RewrittenGoal *rewrites_transform(Expression *goal, int n, ...);

static Map *ptr_counter = NULL;
void print_ptr_counts(void);

#endif