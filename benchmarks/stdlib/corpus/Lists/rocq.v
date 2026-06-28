(* Lists: Tier-A structural-induction units over `list A` (Coq.Init.Datatypes /
   Coq.Lists.List).  These exercise parametric induction — `apply (list_ind A
   <motive>)` — which is reachable now that the kernel reduces a fixpoint over a
   parametric constructor (`app A (cons A x xs) k`) and preserves a match
   parameter slot's delta alias under substitution.  `Coq.Init` is auto-loaded,
   so the unit needs no Require; the list notations live in list_scope.

   `induction l` keeps any binder after `l` (here `m`, `n`) quantified in both the
   goal and the induction hypothesis, so each case introduces them first.  The
   cons case then uses `f_equal` to peel the shared head (`cons x _`) / successor
   (`S _`), mirroring the standard library's own proofs (`induction l; simpl;
   f_equal; auto`); MEngine's compat prelude supplies a single-layer `f_equal`
   (compat/stdlib_compat.me).  Where the stdlib closes the peeled argument goal
   with `auto`, here it is discharged explicitly with the induction hypothesis
   (MEngine's `apply` cannot instantiate the quantified, redex-typed IH, so the
   IH is applied by hand via `exact (IHl m n)`). *)

Open Scope list_scope.

Lemma app_nil_l : forall (A : Type) (l : list A), nil ++ l = l.
Proof. intros A l. reflexivity. Qed.

Lemma app_nil_r : forall (A : Type) (l : list A), l ++ nil = l.
Proof. induction l as [| x l IHl].
  - reflexivity.
  - simpl. f_equal. exact IHl.
Qed.

Lemma app_assoc : forall (A : Type) (l m n : list A), l ++ (m ++ n) = (l ++ m) ++ n.
Proof. induction l as [| x l IHl].
  - intro m. intro n. reflexivity.
  - intro m. intro n. simpl. f_equal. exact (IHl m n).
Qed.

Lemma length_app : forall (A : Type) (l m : list A), length (l ++ m) = length l + length m.
Proof. induction l as [| x l IHl].
  - intro m. reflexivity.
  - intro m. simpl. f_equal. exact (IHl m).
Qed.
