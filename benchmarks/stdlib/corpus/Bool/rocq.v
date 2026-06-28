(* Bool: Tier-A units drawn from Coq.Bool.Bool (operators from Coq.Init.Datatypes). *)

Lemma andb_true_l : forall b : bool, andb true b = b.
Proof. intro b. reflexivity. Qed.

Lemma andb_true_r : forall b : bool, andb b true = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma andb_false_l : forall b : bool, andb false b = false.
Proof. intro b. reflexivity. Qed.

Lemma andb_b_b : forall b : bool, andb b b = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_true_l : forall b : bool, orb true b = true.
Proof. intro b. reflexivity. Qed.

Lemma orb_false_l : forall b : bool, orb false b = b.
Proof. intro b. reflexivity. Qed.

Lemma orb_false_r : forall b : bool, orb b false = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_b_b : forall b : bool, orb b b = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.

Example negb_true : negb true = false.
Proof. reflexivity. Qed.

Example negb_false : negb false = true.
Proof. reflexivity. Qed.

Lemma negb_involutive : forall b : bool, negb (negb b) = b.
Proof. destruct b.
  - reflexivity.
  - reflexivity.
Qed.
