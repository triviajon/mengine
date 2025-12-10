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

// Given a hole and a lemma, use unification to match the expected type of the hole with the type of the lemma,
// and instantiate the lemma with the variables in the hole.
TacticResult *apply_tactic(Expression *goal, Expression *lemma);

// Search the context for a variable whose type matches the goal, and fill the hole with it.
TacticResult *assumption_tactic(Expression *goal);

// Given a hole and a proof term, check if the term's type matches the goal, and fill the hole with it.
TacticResult *exact_tactic(Expression *goal, Expression *proof_term);


#endif  // NEW_TACTICS
