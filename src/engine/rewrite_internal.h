#ifndef REWRITE_INTERNAL_H
#define REWRITE_INTERNAL_H

#include "src/kernel/expression.h"

typedef struct {
    Expression *original;
    Expression *rewritten;
    DoublyLinkedList *new_goals;
    Expression *original_to_rewritten_proof;
} RewriteResult;

RewriteResult *init_rewrite_result(Expression *original, Expression *rewritten,
                                   DoublyLinkedList *new_goals,
                                   Expression *original_to_rewritten_proof);
void free_rewrite_result(RewriteResult *rwr);

RewriteResult *rewrite(Expression *expr, Expression *lemma, Context *context);

#endif  // REWRITE_INTERNAL_H
