#ifndef REWRITE_INTERNAL_H
#define REWRITE_INTERNAL_H

#include <stdbool.h>

#include "src/kernel/expression.h"

/* ============================================================================
 * Relation Registry - tracks (relation, refl, trans, congr) tuples
 * ============================================================================ */

typedef struct {
    Expression *relation;  // e.g. eq
    Expression *refl;      // e.g. eq_refl   : forall A x, R A x x
    Expression *trans;     // e.g. eq_trans   : forall A x y z, R A x y -> R A y z -> R A x z
    Expression *congr;  // e.g. Bad_App_Congruence : forall A B f g x y, R (A->B) f g -> R A x y ->
                        // R B (f x) (g y)
} RelationInfo;

typedef struct RelationRegistry RelationRegistry;

RelationRegistry *relation_registry_new(void);
void relation_registry_free(RelationRegistry *reg);
bool relation_registry_add(RelationRegistry *reg, RelationInfo info);
RelationInfo *relation_registry_lookup(RelationRegistry *reg, Expression *relation);

/* ============================================================================
 * Rewrite Result
 * ============================================================================ */

typedef struct RewriteResult RewriteResult;

struct RewriteResult {
    Expression *original;
    Expression *rewritten;
    DoublyLinkedList *new_goals;
    Expression *original_to_rewritten_proof;
};

RewriteResult *init_rewrite_result(Expression *original, Expression *rewritten,
                                   DoublyLinkedList *new_goals,
                                   Expression *original_to_rewritten_proof);
void free_rewrite_result(RewriteResult *rwr);

/* Accessor functions */
Expression *rewrite_result_get_original(RewriteResult *result);
Expression *rewrite_result_get_rewritten(RewriteResult *result);
DoublyLinkedList *rewrite_result_get_new_goals(RewriteResult *result);

RewriteResult *rewrite(Expression *expr, Expression *lemma, Context *context);
RewriteResult *erewrite(Expression *expr, Expression *lemma, Context *context);

/* Debug stats */
void rewrite_set_debug(bool enabled);
void rewrite_print_cumulative_stats(void);

#endif  // REWRITE_INTERNAL_H
