#ifndef NEW_TACTICS_H
#define NEW_TACTICS_H

#include "src/kernel/expression.h"

typedef struct {
    bool success;                 // true if the tactic was successful, false otherwise
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

// Given a hole and a lemma, use unification to match the expected type of the hole with the type of
// the lemma, and instantiate the lemma with the variables in the hole.
TacticResult *apply_tactic(Expression *goal, Expression *lemma);

// Given a hole and a lemma, use unification to match the expected type of the hole with the type of
// the lemma, and instantiate the lemma with the variables in the hole. For any binders in the
// lemma, this will create existential variables if necessary to match the goal.
TacticResult *eapply_tactic(Expression *goal, Expression *lemma);

// Search the context for a variable whose type matches the goal, and fill the hole with it.
TacticResult *assumption_tactic(Expression *goal);

// Given a hole and a proof term, check if the term's type matches the goal, and fill the hole with
// it.
TacticResult *exact_tactic(Expression *goal, Expression *proof_term);

typedef struct {
    Expression *original;
    Expression *rewritten;
    DoublyLinkedList *new_goals;
    Expression *original_to_rewritten_proof;
} RewriteResult;

// Given a hole and a lemma, where the hole's expected type has the form eq A x y. The eventual goal
// is to generalize this to any equivalence relation. See:
// https://sozeau.gitlabpages.inria.fr/www/research/publications/A_New_Look_at_Generalized_Rewriting_in_Type_Theory.pdf
TacticResult *rewrite_tactic(Expression *goal, Expression *lemma);

#endif  // NEW_TACTICS