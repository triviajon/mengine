Require Import Setoid Morphisms.
Section Test.

Variable nat : Type.
Variable eq : (forall (A: Type), (forall (_: A), (forall (_: A), Prop))).
Variable eq_refl : (forall (B: Type), (forall (x: B), (((eq B) x) x))).
Variable app_cong : (forall (A: Type), (forall (B: Type), (forall (f: (forall (_: A), B)), (forall (g: (forall (_: A), B)), (forall (x: A), (forall (y: A), (forall (_: (((eq (forall (_: A), B)) f) g)), (forall (_: (((eq A) x) y)), (((eq B) (f x)) (g y)))))))))).
Variable eq_trans : (forall (x: nat), (forall (y: nat), (forall (z: nat), (forall (_: (((eq nat) x) y)), (forall (_: (((eq nat) y) z)), (((eq nat) x) z)))))).
Variable b : nat.
Variable f : (forall (_: nat), nat).
Variable a : nat.
Variable eq_fa_a : (forall (a: nat), (((eq nat) (f a)) a)).
Variable g : (forall (_: nat), nat).

Instance Equivalence_eq (A : Type) : Equivalence (eq A). Proof. Admitted.
Instance f_Proper : Proper (eq nat ==> eq nat) f. Proof. Admitted.
Instance g_Proper : Proper (eq nat ==> eq nat) g. Proof. Admitted.
Add Setoid nat (eq nat) (Equivalence_eq nat) as eq_setoid_nat.
