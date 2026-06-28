(* Bool: Tier-A units from Coq.Init.Datatypes / Coq.Bool.Bool — decidable
   identities closed by ground reduction or single boolean case analysis. *)

Lemma andb_true_l : forall b : bool, andb true b = b.
Proof. intro b. reflexivity. Qed.

Lemma andb_false_l : forall b : bool, andb false b = false.
Proof. intro b. reflexivity. Qed.

Lemma andb_true_r : forall b : bool, andb b true = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma andb_false_r : forall b : bool, andb b false = false.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma andb_b_b : forall b : bool, andb b b = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_true_l : forall b : bool, orb true b = true.
Proof. intro b. reflexivity. Qed.

Lemma orb_false_l : forall b : bool, orb false b = b.
Proof. intro b. reflexivity. Qed.

Lemma orb_true_r : forall b : bool, orb b true = true.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_false_r : forall b : bool, orb b false = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_b_b : forall b : bool, orb b b = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma negb_true : negb true = false.
Proof. reflexivity. Qed.

Lemma negb_false : negb false = true.
Proof. reflexivity. Qed.

Lemma negb_involutive : forall b : bool, negb (negb b) = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma negb_involutive_reverse : forall b : bool, b = negb (negb b).
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma andb_negb_r : forall b : bool, andb b (negb b) = false.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma orb_negb_r : forall b : bool, orb b (negb b) = true.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma implb_true_l : forall b : bool, implb true b = b.
Proof. intro b. reflexivity. Qed.

Lemma implb_false_l : forall b : bool, implb false b = true.
Proof. intro b. reflexivity. Qed.

Lemma implb_b_b : forall b : bool, implb b b = true.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma xorb_false_l : forall b : bool, xorb false b = b.
Proof. intro b. reflexivity. Qed.

Lemma xorb_false_r : forall b : bool, xorb b false = b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma xorb_true_l : forall b : bool, xorb true b = negb b.
Proof. intro b. reflexivity. Qed.

Lemma xorb_true_r : forall b : bool, xorb b true = negb b.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.

Lemma xorb_b_b : forall b : bool, xorb b b = false.
Proof. induction b.
  - reflexivity.
  - reflexivity.
Qed.
