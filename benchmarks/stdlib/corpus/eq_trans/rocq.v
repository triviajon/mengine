Lemma eq_trans_ex : forall (A : Type) (x y z : A), x = y -> y = z -> x = z.
Proof. intros A x y z H1 H2. rewrite H1. exact H2. Qed.
