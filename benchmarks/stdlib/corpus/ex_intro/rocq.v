Lemma ex_intro_ex : forall (A : Type) (P : A -> Prop) (x : A), P x -> exists y, P y.
Proof. intros A P x H. exists x. exact H. Qed.
