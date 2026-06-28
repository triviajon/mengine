Lemma or_introl_ex : forall A B : Prop, A -> A \/ B.
Proof. intros A B HA. left; assumption. Qed.
