Lemma ex_eq : forall (A : Type) (x : A), exists y, y = x.
Proof. intros A x. exists x. reflexivity. Qed.
