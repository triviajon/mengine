#ifndef NEW_TACTICS_H
#define NEW_TACTICS_H

#include "src/engine/tactic_api.h"
#include "src/kernel/kernel_api.h"

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

// Given a hole and a lemma, where the hole's expected type has the form eq A x y. The eventual goal
// is to generalize this to any equivalence relation. See:
// https://sozeau.gitlabpages.inria.fr/www/research/publications/A_New_Look_at_Generalized_Rewriting_in_Type_Theory.pdf
TacticResult *rewrite_tactic(Expression *goal, Expression *lemma);

// Same as rewrite_tactic, but allows unresolved binders to be added as goals to the proof state.
TacticResult *erewrite_tactic(Expression *goal, Expression *lemma);

// Given a goal, and optionally a list of conversion rules, attempt to normalize the goal by
// applying the set of conversion rules given in `rules`.
//
// Supported rules:
// - beta: (fun x => t) u ~> t[x := u]
// - delta: unfold definitions
// - iota: match on constructors
// - fix: fixpoint to lambdas
//
// If `rules` is NULL, then all rules are applied
TacticResult *cbv_tactic(Expression *goal, char **rules, int rule_count);

#endif  // NEW_TACTICS