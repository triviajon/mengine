(* Lists: Tier-A structural-induction units over `list A` (Coq.Init.Datatypes /
   Coq.Lists.List).  These exercise parametric induction — `apply (list_ind A
   <motive>)` — which is reachable now that the kernel reduces a fixpoint over a
   parametric constructor (`app A (cons A x xs) k`) and preserves a match
   parameter slot's delta alias under substitution.  app/length are in
   `Coq.Init.Datatypes` (auto-loaded), but `map`/`rev` live in `Coq.Lists.List`,
   so this module (alone in the corpus) `Require`s it — and the benchmark times
   that load against this module's *own* Require/Import preamble baseline, not
   the empty-Prelude floor, so the library load is subtracted rather than charged
   to the proof.

   `induction l` keeps any binder after `l` (here `m`, `n`) quantified in both the
   goal and the induction hypothesis, so each case introduces them first.  The
   cons case then uses `f_equal` to peel the shared head (`cons x _`) / successor
   (`S _`), mirroring the standard library's own proofs (`induction l; simpl;
   f_equal; auto`); MEngine's compat prelude supplies a single-layer `f_equal`
   (compat/stdlib_compat.me).  Where the stdlib closes the peeled argument goal
   with `auto`, here it is discharged explicitly with the induction hypothesis
   (MEngine's `apply` cannot instantiate the quantified, redex-typed IH, so the
   IH is applied by hand via `exact (IHl m n)`). *)

From Stdlib Require Import Lists.List.
Import ListNotations.
Open Scope list_scope.

Lemma app_nil_l : forall (A : Type) (l : list A), nil ++ l = l.
Proof. intros A l. reflexivity. Qed.

Lemma app_nil_r : forall (A : Type) (l : list A), l ++ nil = l.
Proof. induction l as [| x l IHl].
  - reflexivity.
  - simpl; f_equal; exact IHl.
Qed.

Lemma app_comm_cons : forall (A : Type) (l m : list A) (a : A), a :: (l ++ m) = (a :: l) ++ m.
Proof. intros A l m a. reflexivity. Qed.

Lemma app_assoc : forall (A : Type) (l m n : list A), l ++ (m ++ n) = (l ++ m) ++ n.
Proof. induction l as [| x l IHl].
  - intro m. intro n. reflexivity.
  - intro m. intro n. simpl; f_equal; exact (IHl m n).
Qed.

Lemma length_app : forall (A : Type) (l m : list A), length (l ++ m) = length l + length m.
Proof. induction l as [| x l IHl].
  - intro m. reflexivity.
  - intro m. simpl; f_equal; exact (IHl m).
Qed.

Lemma map_app : forall (A B : Type) (f : A -> B) (l m : list A), map f (l ++ m) = map f l ++ map f m.
Proof. induction l as [| x l IHl].
  - intro m. reflexivity.
  - intro m. simpl; f_equal; exact (IHl m).
Qed.

Lemma length_map : forall (A B : Type) (f : A -> B) (l : list A), length (map f l) = length l.
Proof. induction l as [| x l IHl].
  - reflexivity.
  - simpl; f_equal; exact IHl.
Qed.

Lemma rev_app_distr : forall (A : Type) (l m : list A), rev (l ++ m) = rev m ++ rev l.
Proof. induction l as [| x l IHl].
  - intro m. simpl; symmetry; rewrite app_nil_r; reflexivity.
  - intro m. simpl; rewrite IHl; symmetry; rewrite app_assoc; reflexivity.
Qed.

Lemma rev_involutive : forall (A : Type) (l : list A), rev (rev l) = l.
Proof. induction l as [| x l IHl].
  - reflexivity.
  - simpl; rewrite rev_app_distr; simpl; rewrite IHl; reflexivity.
Qed.
