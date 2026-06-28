Lemma f_equal2_ex : forall (A B C : Type) (f : A -> B -> C) (x1 y1 : A) (x2 y2 : B), x1 = y1 -> x2 = y2 -> f x1 x2 = f y1 y2.
Proof. intros A B C f x1 y1 x2 y2 H1 H2. rewrite H1. rewrite H2. reflexivity. Qed.
