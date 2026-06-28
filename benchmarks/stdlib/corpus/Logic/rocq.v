(* Logic: Tier-A units drawn from Coq.Init.Logic. *)

Lemma eq_refl_x : forall (A : Type) (x : A), x = x.
Proof. intros A x. reflexivity. Qed.

Lemma eq_sym_ex : forall (A : Type) (x y : A), x = y -> y = x.
Proof. intros A x y H. symmetry; exact H. Qed.

Lemma eq_trans_ex : forall (A : Type) (x y z : A), x = y -> y = z -> x = z.
Proof. intros A x y z H1 H2. rewrite H1; exact H2. Qed.

Lemma f_equal_ex : forall (A B : Type) (f : A -> B) (x y : A), x = y -> f x = f y.
Proof. intros A B f x y H. rewrite H; reflexivity. Qed.

Lemma f_equal2_ex : forall (A B C : Type) (f : A -> B -> C) (x1 y1 : A) (x2 y2 : B), x1 = y1 -> x2 = y2 -> f x1 x2 = f y1 y2.
Proof. intros A B C f x1 y1 x2 y2 H1 H2. rewrite H1; rewrite H2; reflexivity. Qed.

Lemma f_equal3_ex : forall (A1 A2 A3 B : Type) (f : A1 -> A2 -> A3 -> B) (x1 y1 : A1) (x2 y2 : A2) (x3 y3 : A3), x1 = y1 -> x2 = y2 -> x3 = y3 -> f x1 x2 x3 = f y1 y2 y3.
Proof. intros A1 A2 A3 B f x1 y1 x2 y2 x3 y3 H1 H2 H3. rewrite H1; rewrite H2; rewrite H3; reflexivity. Qed.

Lemma imp_refl : forall A : Prop, A -> A.
Proof. intro A. intro H. exact H. Qed.

Lemma imp_trans : forall A B C : Prop, (A -> B) -> (B -> C) -> A -> C.
Proof. intros A B C HAB HBC HA. apply HBC; apply HAB; exact HA. Qed.

Lemma and_intro : forall A B : Prop, A -> B -> A /\ B.
Proof. intros A B HA HB. split; assumption. Qed.

Lemma and_intro3 : forall A B C : Prop, A -> B -> C -> A /\ (B /\ C).
Proof. intros A B C HA HB HC. repeat split; assumption. Qed.

Lemma or_introl_ex : forall A B : Prop, A -> A \/ B.
Proof. intros A B HA. left; assumption. Qed.

Lemma or_intror_ex : forall A B : Prop, B -> A \/ B.
Proof. intros A B HB. right; assumption. Qed.

Lemma ex_intro_ex : forall (A : Type) (P : A -> Prop) (x : A), P x -> exists y, P y.
Proof. intros A P x H. exists x; exact H. Qed.

Lemma ex_eq : forall (A : Type) (x : A), exists y, y = x.
Proof. intros A x. exists x; reflexivity. Qed.
