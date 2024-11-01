Require Import Setoid Morphisms.

Variable nat : Type.
Variable eq : forall (_ : nat) (_ : nat), Prop.
Variable eq_refl : forall (x : nat), ((eq x) x).
Variable f_equal : forall (f : forall (_ : nat), nat) (x : nat) (y : nat) (_ : ((eq x) y)), ((eq (f x)) (f y)).
Variable eq_trans : forall (x : nat) (y : nat) (z : nat) (_ : ((eq x) y)) (_ : ((eq y) z)), ((eq x) z).
Variable f : forall (_ : nat), nat.
Variable a : nat.
Variable eq_fa_a : ((eq (f a)) a).
Variable g : forall (_ : nat), nat.
Declare Instance Equivalence_eq : Equivalence eq.
Instance f_Proper : Proper (eq ==> eq) f := f_equal f.
Instance g_Proper : Proper (eq ==> eq) g := f_equal g.
