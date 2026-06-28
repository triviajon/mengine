Lemma andb_b_b : forall b : bool, andb b b = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
