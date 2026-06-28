(* Nat: Tier-A units drawn from Coq.Init.Nat (notations from Coq.Init.Peano). *)

Lemma add_0_l : forall n : nat, 0 + n = n.
Proof. intro n. reflexivity. Qed.

Example add_2_2 : 2 + 2 = 4.
Proof. reflexivity. Qed.

Example add_3_4 : 3 + 4 = 7.
Proof. reflexivity. Qed.

Example mul_2_3 : 2 * 3 = 6.
Proof. reflexivity. Qed.

Example sub_5_2 : 5 - 2 = 3.
Proof. reflexivity. Qed.
