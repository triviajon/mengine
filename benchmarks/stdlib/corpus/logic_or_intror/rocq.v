Lemma or_intror_ex : forall A B : Prop, B -> A \/ B.
Proof. intros A B HB. right; assumption. Qed.
