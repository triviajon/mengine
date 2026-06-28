Lemma imp_trans : forall A B C : Prop, (A -> B) -> (B -> C) -> A -> C.
Proof. intros A B C HAB HBC HA. apply HBC. apply HAB. exact HA. Qed.
