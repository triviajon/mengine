#ifndef REWRITES_H
#define REWRITES_H

#include "axiom.h"
#include "beta_reduction.h"
#include "context.h"
#include "logic.h"
#include "rewrite_proof.h"
#include "rewrites.h"
#include "unify.h"
#include "utils.h"

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