Lemma negb_involutive : forall b : bool, negb (negb b) = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
