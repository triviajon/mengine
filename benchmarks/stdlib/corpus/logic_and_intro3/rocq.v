Lemma and_intro3 : forall A B C : Prop, A -> B -> C -> A /\ (B /\ C).
Proof. intros A B C HA HB HC. split. exact HA. split. exact HB. exact HC. Qed.
