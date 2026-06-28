Lemma orb_b_b : forall b : bool, orb b b = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
