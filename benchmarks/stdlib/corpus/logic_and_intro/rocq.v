Lemma and_intro : forall A B : Prop, A -> B -> A /\ B.
Proof. intros A B HA HB. split; assumption. Qed.
