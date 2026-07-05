(* Peano: Tier-A units drawn from Coq.Init.Peano (the le order). *)

Lemma le_refl_n : forall n : nat, n <= n.
Proof. intro n. constructor. Qed.

Lemma le_0_n : forall n : nat, 0 <= n.
Proof. induction n.
  - constructor.
  - constructor; exact IHn.
Qed.

Lemma le_succ_diag_r : forall n : nat, n <= S n.
Proof. intro n. repeat constructor. Qed.
