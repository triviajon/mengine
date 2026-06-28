(* Lists: Tier-A structural-induction units over `list A` (Coq.Init.Datatypes /
   Coq.Lists.List).  These exercise parametric induction — `apply (list_ind A
   <motive>)` — which is reachable now that the kernel reduces a fixpoint over a
   parametric constructor (`app A (cons A x xs) k`) and preserves a match
   parameter slot's delta alias under substitution.  `Coq.Init` is auto-loaded,
   so the unit needs no Require; the list notations live in list_scope.

   `induction l` keeps any binder after `l` (here `m`, `n`) quantified in both the
   goal and the induction hypothesis, so each case introduces them before
   rewriting — exactly as the Nat add_assoc proof does for its `m`/`p`. *)

Open Scope list_scope.

Lemma app_nil_l : forall (A : Type) (l : list A), nil ++ l = l.
Proof. intros A l. reflexivity. Qed.

Lemma app_nil_r : forall (A : Type) (l : list A), l ++ nil = l.
Proof. induction l as [| x l IHl].
  - reflexivity.
  - simpl. rewrite IHl. reflexivity.
Qed.

Lemma app_assoc : forall (A : Type) (l m n : list A), l ++ (m ++ n) = (l ++ m) ++ n.
Proof. induction l as [| x l IHl].
  - intro m. intro n. reflexivity.
  - intro m. intro n. simpl. rewrite IHl. reflexivity.
Qed.

Lemma length_app : forall (A : Type) (l m : list A), length (l ++ m) = length l + length m.
Proof. induction l as [| x l IHl].
  - intro m. reflexivity.
  - intro m. simpl. rewrite IHl. reflexivity.
Qed.
