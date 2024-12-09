Require Import Setoid Morphisms.

Section Test.

Variable nat : Type.
Variable eq : (forall (A: Type), (forall (_: A), (forall (_: A), Prop))).
Variable eq_refl : (forall (B: Type), (forall (x: B), (((eq B) x) x))).
Variable app_cong : (forall (A: Type), (forall (B: Type), (forall (f: (forall (_: A), B)), (forall (g: (forall (_: A), B)), (forall (x: A), (forall (y: A), (forall (_: (((eq (forall (_: A), B)) f) g)), (forall (_: (((eq A) x) y)), (((eq B) (f x)) (g y)))))))))).
Variable eq_trans : (forall (x: nat), (forall (y: nat), (forall (z: nat), (forall (_: (((eq nat) x) y)), (forall (_: (((eq nat) y) z)), (((eq nat) x) z)))))).
Variable b : nat.
Variable f : (forall (_: nat), nat).
Variable h : nat -> nat -> nat.
Variable a : nat.
Variable eq_haa_a : (forall (a: nat), (eq (nat) (h a a) a)).
Variable eq_fa_a : (forall (a: nat), (((eq nat) (f a)) a)).
Variable g : (forall (_: nat), nat).
Declare Instance Equivalence_eq : Equivalence (eq nat).
Goal eq nat (h (h a a) (h a a)) a.
Proof.
	rewrite !eq_haa_a. reflexivity. Qed. 

