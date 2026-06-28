Lemma andb_true_r : forall b : bool, andb b true = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
