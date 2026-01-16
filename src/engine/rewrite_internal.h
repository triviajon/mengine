#ifndef REWRITE_INTERNAL_H
#define REWRITE_INTERNAL_H

#include "src/kernel/expression.h"

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

#endif  // REWRITE_INTERNAL_H
