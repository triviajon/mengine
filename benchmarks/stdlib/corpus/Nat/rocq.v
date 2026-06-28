(* Nat: Tier-A units from Coq.Init.Nat / Coq.Arith.  Ground evaluation plus
   computational induction over the add/mul fixpoints, now reachable thanks to
   the symbolic-fixpoint reduction fix and quantified-IH rewriting. *)

Example add_2_2 : 2 + 2 = 4.
Proof. reflexivity. Qed.

Example mul_2_3 : 2 * 3 = 6.
Proof. reflexivity. Qed.

Example sub_5_2 : 5 - 2 = 3.
Proof. reflexivity. Qed.

Lemma add_0_l : forall n : nat, 0 + n = n.
Proof. intro n. reflexivity. Qed.

Lemma add_0_r : forall n : nat, n + 0 = n.
Proof. induction n.
  - reflexivity.
  - simpl. rewrite IHn. reflexivity.
Qed.

Lemma add_succ_r : forall n m : nat, n + S m = S (n + m).
Proof. induction n.
  - intro m. reflexivity.
  - intro m. simpl. rewrite IHn. reflexivity.
Qed.

Lemma add_comm : forall n m : nat, n + m = m + n.
Proof. induction n.
  - intro m. simpl. symmetry. rewrite add_0_r. reflexivity.
  - intro m. simpl. rewrite IHn. symmetry. rewrite add_succ_r. reflexivity.
Qed.

Lemma add_assoc : forall n m p : nat, n + (m + p) = (n + m) + p.
Proof. induction n.
  - intro m. intro p. reflexivity.
  - intro m. intro p. simpl. rewrite IHn. reflexivity.
Qed.

Lemma mul_0_r : forall n : nat, n * 0 = 0.
Proof. induction n.
  - reflexivity.
  - simpl. rewrite IHn. reflexivity.
Qed.

Lemma mul_1_l : forall n : nat, 1 * n = n.
Proof. intro n. simpl. rewrite add_0_r. reflexivity. Qed.

Lemma mul_1_r : forall n : nat, n * 1 = n.
Proof. induction n.
  - reflexivity.
  - simpl. rewrite IHn. reflexivity.
Qed.

Lemma eqb_refl : forall n : nat, Nat.eqb n n = true.
Proof. induction n.
  - reflexivity.
  - simpl. rewrite IHn. reflexivity.
Qed.

Lemma leb_refl : forall n : nat, Nat.leb n n = true.
Proof. induction n.
  - reflexivity.
  - simpl. rewrite IHn. reflexivity.
Qed.
