Lemma eq_sym_ex : forall (A : Type) (x y : A), x = y -> y = x.
Proof. intros A x y H. symmetry. exact H. Qed.
