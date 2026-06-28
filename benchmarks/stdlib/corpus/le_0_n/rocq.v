Lemma le_0_n : forall n : nat, 0 <= n.
Proof. induction n.
  - constructor.
  - constructor. exact IHn.
Qed.
