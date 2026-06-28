Lemma orb_false_r : forall b : bool, orb b false = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
