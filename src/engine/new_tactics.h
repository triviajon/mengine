#ifndef NEW_TACTICS_H
#define NEW_TACTICS_H

#include "src/kernel/expression.h"
#include "src/kernel/context.h"

typedef struct {
    bool success; // true if the tactic was successful, false otherwise
    DoublyLinkedList *new_goals;  // If success, the new goals, otherwise NULL
    char *error_message;          // If success, NULL, otherwise the error message
} TacticResult;

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message);
void free_tactic_result(TacticResult *result);

// DoublyLinkedList *apply(Expression *goal, Expression *lemma);
// DoublyLinkedList *eapply(Expression *goal, Expression *lemma);
// Expression *eexists(Expression *goal);

// Given a hole with expected type "forall (x : A), B" and a name, 
// fill the hole with the term "λ x : A, ?B" and the result of the tactic is returned.
TacticResult *intro_tactic(Expression *goal, char *name);

// Given a hole with expected type "forall (x : A), B" and a list of names,
// fill the hole with the term "λ x : A, ?B" and the result of the tactic is returned.
TacticResult *intros_tactic(Expression *goal, char **names, size_t name_count);

#endif  // NEW_TACTICS
