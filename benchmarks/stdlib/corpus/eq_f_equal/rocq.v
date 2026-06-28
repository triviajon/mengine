Lemma f_equal_ex : forall (A B : Type) (f : A -> B) (x y : A), x = y -> f x = f y.
Proof. intros A B f x y H. rewrite H. reflexivity. Qed.
