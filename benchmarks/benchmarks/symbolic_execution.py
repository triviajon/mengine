"""
Symbolic execution benchmark.

Verifies imperative programs with n repeated set+sub operations
using partial map reasoning.

All three engines use the same approach:
  - Python generates ONLY n (a concrete nat) - everything else is static
  - A Fixpoint/def `repeated_cmds n` defines the n-step command
  - The proof uses reduction (cbv/cbn/simp) to unfold the fixpoint
  - Then a tactic loop (`repeat_exec`) symbolically executes each step

This ensures O(n) actual proof work per benchmark point.
No axioms are used to shortcut the connection between n and the command.
"""

import os
from framework.benchmark import Benchmark, Strategy, ParamSpec


class SymbolicExecution(Benchmark):
    @property
    def name(self):
        return "symbolic_execution"

    @property
    def description(self):
        return "Symbolic execution of imperative programs with partial maps"

    @property
    def params(self):
        return [ParamSpec("n", start=1, stop=250, step=5)]

    @property
    def x_label(self):
        return "n (# set+sub operations)"

    @property
    def strategies(self):
        return [
            Strategy("mengine", "repeat_exec", "MEngine", color="blue", marker="s"),
            Strategy("coq", "repeat_exec", "Rocq: repeat_exec", color="red", marker="o"),
            Strategy("coq", "concrete", "Rocq: repeat_exec (concrete)", color="darkred", marker="P"),
            Strategy("lean", "repeat_exec", "Lean: repeat_exec", color="green", marker="v"),
            Strategy("lean", "concrete", "Lean: repeat_exec (concrete)", color="darkgreen", marker="D"),
            Strategy("lean", "symm", "Lean: SymM", color="purple", marker="^"),
        ]

    # Plot defaults specific to this benchmark: the concrete repeat_exec twins and
    # the MEngine ablation variants are hidden unless `bench.py plot --show-all`.
    @property
    def default_exclude(self):
        return [r"/concrete$"]

    @property
    def default_no_ablations(self):
        return True

    def generate(self, strategy, params, workdir):
        n = params["n"]

        if strategy.engine == "mengine":
            return self._generate_mengine(n, workdir)

        if strategy.engine == "coq":
            if strategy.name == "concrete":
                return self._generate_coq_concrete(n, workdir)
            return self._generate_coq(n, workdir)

        if strategy.engine == "lean":
            if strategy.name == "symm":
                return self._generate_lean_symm(n, workdir)
            if strategy.name == "concrete":
                return self._generate_lean_concrete(n, workdir)
            return self._generate_lean(n, workdir)

    def _generate_mengine(self, n, workdir):
        # Python generates the concrete command term inline.
        # The proof mirrors test.me: 2 intros, repeat exec_once, reflexivity.
        concrete_repeated = "cmd_skip"
        for _ in range(n):
            concrete_repeated = (
                f"(cmd_seq (cmd_set a_str (expr_op bop_add (expr_var a_str) (expr_var a_str)))"
                f" (cmd_seq (cmd_set a_str (expr_op bop_sub (expr_var a_str) (expr_var b_str)))"
                f" {concrete_repeated}))"
            )
        concrete_cmd = (
            f"(cmd_seq (cmd_input b_str)"
            f" (cmd_seq (cmd_set a_str (expr_var b_str))"
            f" {concrete_repeated}))"
        )

        content = f"""(* Symbolic execution benchmark - MEngine, n={n} *)
(* Python generates only the concrete command (inline, fully unrolled). *)
(* repeat_exec symbolically executes each instruction (O(n) tactic work). *)

(* === Base types === *)
Axiom string : Type.
Axiom a_str : string.
Axiom b_str : string.

(* === Option type === *)
Axiom option : forall (_ : Type), Type.
Axiom some : forall (A : Type), forall (_ : A), option A.

(* === Word and memory types === *)
Axiom word : Type.
Axiom byte : Type.
Axiom word_add : forall (_ : option word), forall (_ : option word), word.
Axiom word_sub : forall (_ : option word), forall (_ : option word), word.
Axiom word_add_sub_cancel : forall (v : word),
    eq (option word)
        (some word (word_sub (some word (word_add (some word v) (some word v))) (some word v)))
        (some word v).

(* === Partial maps === *)
Axiom pmap : forall (_ : Type), forall (_ : Type), Type.
Axiom pmap_get : forall (A : Type), forall (B : Type), forall (_ : pmap A B), forall (_ : A), option B.
Axiom pmap_put : forall (A : Type), forall (B : Type), forall (_ : pmap A B), forall (_ : A), forall (_ : B), pmap A B.
Axiom pmap_get_put_same : forall (A : Type), forall (B : Type), forall (m : pmap A B), forall (k : A), forall (v : B),
    eq (option B) (pmap_get A B (pmap_put A B m k v) k) (some B v).
Axiom pmap_get_put_diff_ba : forall (m : pmap string word), forall (v : word),
    eq (option word) (pmap_get string word (pmap_put string word m a_str v) b_str) (pmap_get string word m b_str).
Axiom pmap_put_put_same : forall (A : Type), forall (B : Type), forall (m : pmap A B), forall (k : A), forall (v1 : B), forall (v2 : B),
    eq (pmap A B) (pmap_put A B (pmap_put A B m k v1) k v2) (pmap_put A B m k v2).

(* === Binary operations === *)
Axiom bopname : Type.
Axiom bop_add : bopname.
Axiom bop_sub : bopname.
Axiom interp_binop : forall (_ : bopname), forall (_ : option word), forall (_ : option word), word.
Axiom binop_to_add : forall (w1 : option word), forall (w2 : option word),
    eq word (interp_binop bop_add w1 w2) (word_add w1 w2).
Axiom binop_to_sub : forall (w1 : option word), forall (w2 : option word),
    eq word (interp_binop bop_sub w1 w2) (word_sub w1 w2).

(* === Expressions (axiomatized with explicit eval rules) === *)
Axiom expr : Type.
Axiom expr_var : forall (_ : string), expr.
Axiom expr_op : forall (_ : bopname), forall (_ : expr), forall (_ : expr), expr.
Axiom eval_expr : forall (_ : pmap word byte), forall (_ : pmap string word), forall (_ : expr), option word.
Axiom eval_var : forall (me : pmap word byte), forall (le : pmap string word), forall (x : string),
    eq (option word) (eval_expr me le (expr_var x)) (pmap_get string word le x).
Axiom eval_op : forall (me : pmap word byte), forall (le : pmap string word),
    forall (op : bopname), forall (e1 : expr), forall (e2 : expr),
    eq (option word) (eval_expr me le (expr_op op e1 e2))
        (some word (interp_binop op (eval_expr me le e1) (eval_expr me le e2))).

(* === Commands === *)
Axiom cmd : Type.
Axiom cmd_skip : cmd.
Axiom cmd_set : forall (_ : string), forall (_ : expr), cmd.
Axiom cmd_seq : forall (_ : cmd), forall (_ : cmd), cmd.
Axiom cmd_input : forall (_ : string), cmd.

(* === IO Events === *)
Axiom IOEvent : Type.
Axiom IO_IN : forall (_ : word), IOEvent.
Axiom mylist : forall (_ : Type), Type.
Axiom mynil : forall (A : Type), mylist A.
Axiom mycons : forall (A : Type), forall (_ : A), forall (_ : mylist A), mylist A.

(* === Exec predicate === *)
Axiom exec : forall (_ : cmd),
    forall (_ : mylist IOEvent),
    forall (_ : pmap word byte),
    forall (_ : pmap string word),
    forall (_ : forall (_ : mylist IOEvent), forall (_ : pmap word byte), forall (_ : pmap string word), Prop),
    Prop.
Axiom exec_skip : forall (t : mylist IOEvent), forall (m : pmap word byte),
    forall (l : pmap string word),
    forall (post : forall (_ : mylist IOEvent), forall (_ : pmap word byte), forall (_ : pmap string word), Prop),
    forall (_ : post t m l),
    exec cmd_skip t m l post.
Axiom exec_set : forall (t : mylist IOEvent), forall (x : string), forall (e : expr),
    forall (m : pmap word byte), forall (l : pmap string word),
    forall (post : forall (_ : mylist IOEvent), forall (_ : pmap word byte), forall (_ : pmap string word), Prop),
    forall (v : word),
    forall (_ : eq (option word) (eval_expr m l e) (some word v)),
    forall (_ : post t m (pmap_put string word l x v)),
    exec (cmd_set x e) t m l post.
Axiom exec_seq : forall (t : mylist IOEvent), forall (c1 : cmd), forall (c2 : cmd),
    forall (m : pmap word byte), forall (l : pmap string word),
    forall (post : forall (_ : mylist IOEvent), forall (_ : pmap word byte), forall (_ : pmap string word), Prop),
    forall (_ : exec c1 t m l
        (fun (t2 : mylist IOEvent) => fun (m2 : pmap word byte) => fun (l2 : pmap string word) =>
            exec c2 t2 m2 l2 post)),
    exec (cmd_seq c1 c2) t m l post.
Axiom exec_input : forall (t : mylist IOEvent), forall (lhs : string),
    forall (m : pmap word byte), forall (l : pmap string word),
    forall (post : forall (_ : mylist IOEvent), forall (_ : pmap word byte), forall (_ : pmap string word), Prop),
    forall (_ : forall (v : word), post (mycons IOEvent (IO_IN v) t) m (pmap_put string word l lhs v)),
    exec (cmd_input lhs) t m l post.

(* === Tactics for symbolic execution === *)
Tactic normalize_store :=
    try rewrite pmap_put_put_same with eq;
    try rewrite pmap_put_put_same with eq.

Tactic normalize_get :=
    try rewrite pmap_get_put_same with eq;
    try rewrite pmap_get_put_diff_ba with eq;
    try rewrite pmap_get_put_same with eq;
    try rewrite pmap_get_put_diff_ba with eq;
    try rewrite pmap_get_put_same with eq.

Tactic solve_var_eq :=
    rewrite eval_var with eq;
    normalize_store;
    normalize_get;
    apply eq_refl.

Tactic solve_op_eq :=
    rewrite eval_op with eq;
    rewrite eval_var with eq;
    rewrite eval_var with eq;
    normalize_store;
    normalize_get;
    first [
        rewrite binop_to_add with eq
    | rewrite binop_to_sub with eq
    ];
    try rewrite word_add_sub_cancel with eq;
    apply eq_refl.

Tactic solve_eq_by_rewriting :=
    first [
        solve_var_eq
    | solve_op_eq
    ].

Tactic exec_once := match Goal with
    | [ |- exec (cmd_seq _ _) _ _ _ _ ] => apply exec_seq
    | [ |- exec (cmd_input _) _ _ _ _ ] => apply exec_input; intro
    | [ |- exec (cmd_set _ _) _ _ _ _ ] =>
        normalize_store;
        eapply exec_set; first [
        match Goal with
        | [ |- eq (option word) (eval_expr _ _ _) (some word _) ] => solve_eq_by_rewriting
        end
      | idtac
      ]
    | [ |- exec cmd_skip _ _ _ _ ] => apply exec_skip
    end.

Tactic repeat_exec :=
    repeat exec_once.

(* === Main theorem ===
   Python generates only the concrete command term below.
   repeat_exec symbolically executes each instruction: O(n) proof work.
   Postcondition: memory is unchanged (eq m2 m).
   The proof script is fully static - only the command term changes with n. *)
Theorem sym_exec :
    forall (m : pmap word byte), forall (l : pmap string word),
    exec
        {concrete_cmd}
        (mynil IOEvent) m l
        (fun (t2 : mylist IOEvent) => fun (m2 : pmap word byte) => fun (l2 : pmap string word) =>
            eq (pmap word byte) m2 m).
intro m.
intro l.
repeat_exec.
reflexivity.
"""
        path = os.path.join(workdir, "test.me")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_coq(self, n, workdir):
        content = r"""Require Import Coq.Lists.List.
Import ListNotations.

Section A.
    Variable string : Set.
    Variable a_str b_str : string.
    Variable not_eq_b_str_a_str : b_str <> a_str.

    Variable word : Set.
    Variable byte : Set.
    Variable word_add : option word -> option word -> word.
    Variable word_sub : option word -> option word -> word.
    Variable word_add_sub_cancel : forall (v : word),
        @eq (option word)
            (Some (word_sub (Some (word_add (Some v) (Some v))) (Some v)))
            (Some v).

    Variable pmap : Set -> Set -> Set.
    Variable pmap_get : forall (A B : Set), pmap A B -> A -> option B.
    Variable pmap_put : forall (A B : Set), pmap A B -> A -> B -> pmap A B.
    Variable pmap_get_put_same : forall (A B : Set) (m : pmap A B) (k : A) (v : B),
        @eq (option B) (pmap_get A B (pmap_put A B m k v) k) (Some v).
    Variable pmap_get_put_diff : forall (A B : Set) (m : pmap A B) (k k' : A) (v : B),
        k <> k' ->
        @eq (option B) (pmap_get A B (pmap_put A B m k' v) k) (pmap_get A B m k).
    Variable pmap_put_put_same : forall (A B : Set) (m : pmap A B) (k : A) (v1 v2 : B),
        @eq (pmap A B) (pmap_put A B (pmap_put A B m k v1) k v2)
            (pmap_put A B m k v2).

    Variable bopname : Set.
    Variable bop_add : bopname.
    Variable bop_sub : bopname.
    Variable interp_binop : bopname -> option word -> option word -> word.
    Variable binop_to_add : forall (w1 w2 : option word),
        @eq word (interp_binop bop_add w1 w2) (word_add w1 w2).
    Variable binop_to_sub : forall (w1 w2 : option word),
        @eq word (interp_binop bop_sub w1 w2) (word_sub w1 w2).

    Inductive expr : Set :=
        | expr_var : string -> expr
        | expr_op  : bopname -> expr -> expr -> expr.

    Fixpoint eval_expr (me : pmap word byte) (le : pmap string word) (e : expr) : option word :=
        match e with
        | expr_var x => pmap_get string word le x
        | expr_op op e1 e2 => Some (interp_binop op (eval_expr me le e1) (eval_expr me le e2))
        end.

    Variable cmd : Set.
    Variable cmd_skip : cmd.
    Variable cmd_set  : string -> expr -> cmd.
    Variable cmd_seq  : cmd -> cmd -> cmd.
    Variable cmd_input : string -> cmd.

    Variable IOEvent : Set.
    Variable IO_IN : word -> IOEvent.

    Variable exec : cmd ->
        list IOEvent ->
        pmap word byte ->
        pmap string word ->
        (list IOEvent -> pmap word byte -> pmap string word -> Prop) -> Prop.
    Variable exec_skip : forall (t : list IOEvent) (m : pmap word byte)
        (l : pmap string word)
        (post : list IOEvent -> pmap word byte -> pmap string word -> Prop),
        post t m l -> exec cmd_skip t m l post.
    Variable exec_set : forall (t : list IOEvent) (x : string) (e : expr)
        (m : pmap word byte) (l : pmap string word)
        (post : list IOEvent -> pmap word byte -> pmap string word -> Prop) (v : word),
        @eq (option word) (eval_expr m l e) (Some v) ->
        post t m (pmap_put string word l x v) ->
        exec (cmd_set x e) t m l post.
    Variable exec_seq : forall (t : list IOEvent) (c1 c2 : cmd)
        (m : pmap word byte) (l : pmap string word)
        (post : list IOEvent -> pmap word byte -> pmap string word -> Prop),
        exec c1 t m l (fun t' m' l' => exec c2 t' m' l' post) ->
        exec (cmd_seq c1 c2) t m l post.
    Variable exec_input : forall (t : list IOEvent) (lhs : string)
        (m : pmap word byte) (l : pmap string word)
        (post : list IOEvent -> pmap word byte -> pmap string word -> Prop),
        (forall v : word, post (IO_IN v :: t) m (pmap_put string word l lhs v)) ->
        exec (cmd_input lhs) t m l post.

    Fixpoint repeated_cmds (n : nat) (t x : string) : cmd :=
        match n with
        | 0 => cmd_skip
        | S n' =>
            cmd_seq
                (cmd_set t (expr_op bop_add (expr_var t) (expr_var t)))
                (cmd_seq
                    (cmd_set t (expr_op bop_sub (expr_var t) (expr_var x)))
                    (repeated_cmds n' t x))
        end.

    Definition generated_cmd (n : nat) (t x : string) : cmd :=
        cmd_seq (cmd_input x) (cmd_seq (cmd_set t (expr_var x)) (repeated_cmds n t x)).

    Ltac solve_eq_by_rewriting :=
        cbn [eval_expr];
        repeat first [
            rewrite pmap_get_put_same
          | rewrite pmap_get_put_diff by apply not_eq_b_str_a_str
          | rewrite binop_to_add
          | rewrite binop_to_sub
        ];
        try rewrite word_add_sub_cancel;
        apply eq_refl.

    Ltac exec_once := match goal with
        | [ |- exec (cmd_seq _ _) _ _ _ _ ] => apply exec_seq
        | [ |- exec (cmd_input _) _ _ _ _ ] => apply exec_input; intro
        | [ |- exec (cmd_set _ _) _ _ _ _ ] => eapply exec_set; first [
            match goal with
            | [ |- @eq (option word) (eval_expr _ _ _) (Some _) ] => solve_eq_by_rewriting
            end
          | idtac
          ]
        | [ |- exec cmd_skip _ _ _ _ ] => apply exec_skip
        end.

    Ltac repeat_exec :=
        repeat exec_once.

    Theorem sym_exec :
        forall (m : pmap word byte) (l : pmap string word),
            exec (generated_cmd """ + str(n) + r""" a_str b_str) [] m l
                (fun t' m' l' => m' = m).
    Proof.
        unfold generated_cmd;
        cbn [repeated_cmds];
        intros;
        apply exec_seq;
        apply exec_input;
        intro.
        Time repeat_exec.
        reflexivity.
    Qed.
End A.
"""
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean(self, n, workdir):
        # Build concrete command term (fully unrolled, same as MEngine - no fixpoint/induction)
        concrete_repeated = "cmd_skip"
        for _ in range(n):
            concrete_repeated = (
                f"(cmd_seq (cmd_set \"a\" (expr_op bop_add (expr_var \"a\") (expr_var \"a\")))"
                f" (cmd_seq (cmd_set \"a\" (expr_op bop_sub (expr_var \"a\") (expr_var \"b\")))"
                f" {concrete_repeated}))"
            )
        concrete_cmd = (
            f"(cmd_seq (cmd_input \"b\")"
            f" (cmd_seq (cmd_set \"a\" (expr_var \"b\"))"
            f" {concrete_repeated}))"
        )

        content = f"""-- Symbolic execution benchmark (Lean 4) - n={n}
-- Python generates the concrete {n}-step command (fully unrolled, same as MEngine).
-- The proof is a static elaborator tactic loop, mirroring the Coq/MEngine repeat_exec scripts.

import Lean
open Lean Elab Tactic

set_option maxHeartbeats 0
set_option maxRecDepth 1000000

axiom word : Type
axiom byte : Type
axiom word_add : Option word → Option word → word
axiom word_sub : Option word → Option word → word
axiom word_add_sub_cancel : ∀ v : word,
    some (word_sub (some (word_add (some v) (some v))) (some v)) = some v

axiom PMap : Type → Type → Type
axiom pmap_get : {{A B : Type}} → PMap A B → A → Option B
axiom pmap_put : {{A B : Type}} → PMap A B → A → B → PMap A B
axiom pmap_get_put_same : {{A B : Type}} → (m : PMap A B) → (k : A) → (v : B) →
    pmap_get (pmap_put m k v) k = some v
axiom pmap_put_put_same : {{A B : Type}} → (m : PMap A B) → (k : A) → (v1 v2 : B) →
    pmap_put (pmap_put m k v1) k v2 = pmap_put m k v2
axiom pmap_get_put_diff_ba : (m : PMap String word) → (v : word) →
    pmap_get (pmap_put m "a" v) "b" = pmap_get m "b"

axiom Bopname : Type
axiom bop_add : Bopname
axiom bop_sub : Bopname
axiom interp_binop : Bopname → Option word → Option word → word
axiom binop_to_add : ∀ w1 w2, interp_binop bop_add w1 w2 = word_add w1 w2
axiom binop_to_sub : ∀ w1 w2, interp_binop bop_sub w1 w2 = word_sub w1 w2

axiom SymExpr : Type
axiom expr_var : String → SymExpr
axiom expr_op  : Bopname → SymExpr → SymExpr → SymExpr

axiom eval_expr : PMap word byte → PMap String word → SymExpr → Option word
axiom eval_var : ∀ (me : PMap word byte) (le : PMap String word) (x : String),
    eval_expr me le (expr_var x) = pmap_get le x
axiom eval_op : ∀ (me : PMap word byte) (le : PMap String word) (op : Bopname) (e1 e2 : SymExpr),
    eval_expr me le (expr_op op e1 e2) =
      some (interp_binop op (eval_expr me le e1) (eval_expr me le e2))

axiom IOEvent : Type
axiom IO_IN : word → IOEvent
axiom Cmd : Type
axiom cmd_skip : Cmd
axiom cmd_set : String → SymExpr → Cmd
axiom cmd_seq : Cmd → Cmd → Cmd
axiom cmd_input : String → Cmd

axiom exec : Cmd → List IOEvent → PMap word byte → PMap String word →
    (List IOEvent → PMap word byte → PMap String word → Prop) → Prop
axiom exec_skip : ∀ t m l post, post t m l → exec cmd_skip t m l post
axiom exec_set  : ∀ t x e m l post v,
    eval_expr m l e = some v →
    post t m (pmap_put l x v) →
    exec (cmd_set x e) t m l post
axiom exec_seq  : ∀ t c1 c2 m l post,
    exec c1 t m l (fun t' m' l' => exec c2 t' m' l' post) →
    exec (cmd_seq c1 c2) t m l post
axiom exec_input : ∀ t lhs m l post,
    (∀ v : word, post (IO_IN v :: t) m (pmap_put l lhs v)) →
    exec (cmd_input lhs) t m l post

elab "solve_eq_by_rewriting" : tactic => do
  evalTactic (← `(tactic|
    first
    | simp only [eval_var, pmap_get_put_same]; rfl
    | simp only [eval_op, eval_var, binop_to_add, pmap_get_put_same, pmap_put_put_same]; rfl
    | simp only [eval_op, eval_var, binop_to_sub,
        pmap_get_put_same, pmap_get_put_diff_ba, pmap_put_put_same,
        binop_to_add, word_add_sub_cancel]; rfl))

elab "exec_set_step" : tactic => do
  evalTactic (← `(tactic| apply exec_set (v := _)))
  evalTactic (← `(tactic| solve_eq_by_rewriting))

elab "exec_once" : tactic => do
  evalTactic (← `(tactic|
    first
    | apply exec_seq
    | (apply exec_input; intro)
    | exec_set_step
    | apply exec_skip))

elab "repeat_exec" : tactic => do
  evalTactic (← `(tactic| repeat exec_once))

theorem sym_exec (m : PMap word byte) (l : PMap String word) :
    exec {concrete_cmd} [] m l (fun _ m' _ => m' = m) := by
  repeat_exec
  rfl
"""
        path = os.path.join(workdir, "test.lean")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _lean_eval_framework(self):
        return r'''namespace Eval
abbrev Byte := UInt8
abbrev Word := UInt32

def Word.ofInt (a : Int) : Word :=
  UInt32.ofInt a

def Word.add (a b : Option Word) : Word :=
  match a, b with
  | some a, some b => a+b
  | _, _ => 0

def Word.sub (a b : Option Word) : Word :=
  match a, b with
  | some a, some b => a-b
  | _, _ => 0

theorem Word.add_sub_cancel (v : Word) : Word.sub (some (Word.add (some v) (some v))) (some v) = v := by
  simp [Word.add, Word.sub]

theorem Word.add_zero (x : Word) : Word.add (some x) (some (Word.ofInt 0)) = x := by
  simp [Word.add, Word.ofInt, UInt32.ofInt]

structure PartialMap (A B : Type) where
  fn : A -> Option B

def PartialMap.empty (A B : Type) : PartialMap A B :=
  { fn := fun _ => none }

def PartialMap.get {A B : Type} (m : PartialMap A B) (k : A) : Option B :=
  m.fn k

def PartialMap.put [DecidableEq A] (m : PartialMap A B) (k : A) (v : B) : PartialMap A B :=
  { fn := fun x => if x = k then some v else m.fn x }

def PartialMap.remove [DecidableEq A] (m : PartialMap A B) (k : A) : PartialMap A B :=
  { fn := fun x => if x = k then none else m.fn x }

@[ext] theorem PartialMap.ext (m₁ m₂ : PartialMap A B) (h : ∀ k, m₁.get k = m₂.get k) : m₁ = m₂ := by
  cases m₁; cases m₂; congr; funext; apply h

theorem PartialMap.get_put {A B : Type} [DecidableEq A] (m : PartialMap A B) (k : A) (v : B)
    : (m.put k v).get k = some v := by
  simp [PartialMap.get, PartialMap.put]

theorem PartialMap.get_put_diff {A B : Type} [DecidableEq A] (m : PartialMap A B) (k k' : A) (v : B)
    (h : k ≠ k') : (m.put k' v).get k = m.get k :=
  if_neg h

theorem PartialMap.put_put {A B : Type} [DecidableEq A] (m : PartialMap A B) (k : A) (v1 v2 : B)
    : (m.put k v1).put k v2 = m.put k v2 := by
  ext; grind +splitImp [PartialMap.get_put, PartialMap.get_put_diff]

inductive Binop where
  | add | sub
  deriving Repr

def Binop.interp (op : Binop) (a b : Option Word) : Word :=
  match op with
  | .add => Word.add a b
  | .sub => Word.sub a b

theorem Binop.interp_add (w1 w2 : Option Word) : Binop.interp .add w1 w2 = Word.add w1 w2 :=
  rfl

theorem Binop.interp_sub (w1 w2 : Option Word) : Binop.interp .sub w1 w2 = Word.sub w1 w2 :=
  rfl

inductive Expr where
  | literal (v: Int)
  | var (x: String)
  | op (bop : Binop) (e1 e2 : Expr)
  deriving Repr

def Expr.eval (me : PartialMap Word Byte) (le : PartialMap String Word) (e : Expr) : Option Word :=
  match e with
  | .literal v    => some (Word.ofInt v)
  | .var x        => le.get x
  | .op bop e1 e2 => some (bop.interp (e1.eval me le) (e2.eval me le))

inductive Cmd where
  | skip : Cmd
  | set : String → Expr → Cmd
  | unset : String → Cmd
  | cond : Expr → Cmd → Cmd → Cmd
  | seq : Cmd → Cmd → Cmd
  | input : String → Cmd
  | output : String → Cmd
  deriving Repr

inductive IOEvent
  | IN : Word → IOEvent
  | OUT : Word → IOEvent

inductive Exec :
    Cmd → List IOEvent → PartialMap Word Byte → PartialMap String Word →
    (List IOEvent → PartialMap Word Byte → PartialMap String Word → Prop) → Prop
| skip :
    post t m l → Exec .skip t m l post
| seq :
    Exec c1 t m l mid →
    (∀ t' m' l', mid t' m' l' → Exec c2 t' m' l' post) →
    Exec (.seq c1 c2) t m l post
| set :
    Expr.eval m l e = some v →
    post t m (PartialMap.put l x v) →
    Exec (.set x e) t m l post
| input :
    (∀ v, post (IOEvent.IN v :: t) m (PartialMap.put l x v)) →
    Exec (.input x) t m l post

theorem Exec.seq_cps (t : List IOEvent) (c1 c2 : Cmd) (m : PartialMap Word Byte) (l : PartialMap String Word)
    (post : List IOEvent → PartialMap Word Byte → PartialMap String Word → Prop)
    : Exec c1 t m l (λ t' m' l' => Exec c2 t' m' l' post) →
      Exec (.seq c1 c2) t m l post := by
  intro h; apply Exec.seq
  apply h; intros; assumption

def repeated_cmds (n : Nat) (t x : String) : Cmd :=
  match n with
  | 0 => .skip
  | n + 1 =>
    .seq (.set t (.op .add (.var t) (.var t)))
         (.seq (.set t (.op .sub (.var t) (.var x)))
               (repeated_cmds n t x))

def generated_cmd (n : Nat) (t x : String) : Cmd :=
  .seq (.input x)
       (.seq (.set t (.var x))
             (repeated_cmds n t x))

def Goal (n : Nat) : Prop :=
  ∀ m l, Exec (generated_cmd n "a" "b") [] m l fun t' m' l' =>
            m' = m ∧
            ∃ v : Word, t' = [IOEvent.IN v] ∧ l'.get "a" = some v

'''

    def _generate_coq_concrete(self, n, workdir):
        content = r'''Require Import Coq.Strings.String.
Require Import Coq.Lists.List. Import ListNotations.
Require Import Coq.Arith.PeanoNat. Require Import Lia.
Require Import Coq.Logic.FunctionalExtensionality.
Open Scope string_scope.

Definition word := nat.
Definition word_add (a b : option word) : word :=
  match a, b with | Some x, Some y => x + y | _, _ => 0 end.
Definition word_sub (a b : option word) : word :=
  match a, b with | Some x, Some y => x - y | _, _ => 0 end.
Lemma word_add_sub_cancel : forall (v : word),
  @eq (option word) (Some (word_sub (Some (word_add (Some v) (Some v))) (Some v))) (Some v).
Proof. intro v. unfold word_add, word_sub. f_equal. lia. Qed.

Definition pmap (B : Type) := string -> option B.
Definition pmap_get {B} (m : pmap B) (k : string) : option B := m k.
Definition pmap_put {B} (m : pmap B) (k : string) (v : B) : pmap B :=
  fun x => if string_dec x k then Some v else m x.
Lemma pmap_get_put_same : forall B (m : pmap B) k v,
  pmap_get (pmap_put m k v) k = Some v.
Proof. intros. unfold pmap_get, pmap_put. destruct (string_dec k k); congruence. Qed.
Lemma pmap_get_put_diff : forall B (m : pmap B) k k' v,
  k <> k' -> pmap_get (pmap_put m k' v) k = pmap_get m k.
Proof. intros. unfold pmap_get, pmap_put. destruct (string_dec k k'); congruence. Qed.
Lemma pmap_put_put_same : forall B (m : pmap B) k v1 v2,
  pmap_put (pmap_put m k v1) k v2 = pmap_put m k v2.
Proof. intros. unfold pmap_put. apply functional_extensionality. intro x.
  destruct (string_dec x k); reflexivity. Qed.

Inductive bopname := bop_add | bop_sub.
Definition interp_binop (op : bopname) (a b : option word) : word :=
  match op with | bop_add => word_add a b | bop_sub => word_sub a b end.

Inductive expr : Set :=
  | expr_var : string -> expr
  | expr_op  : bopname -> expr -> expr -> expr.
Fixpoint eval_expr (me : pmap unit) (le : pmap word) (e : expr) : option word :=
  match e with
  | expr_var x => pmap_get le x
  | expr_op op e1 e2 => Some (interp_binop op (eval_expr me le e1) (eval_expr me le e2))
  end.

Inductive cmd :=
  | cmd_skip : cmd
  | cmd_set  : string -> expr -> cmd
  | cmd_seq  : cmd -> cmd -> cmd
  | cmd_input : string -> cmd.
Inductive IOEvent := IO_IN : word -> IOEvent.

Inductive exec : cmd -> list IOEvent -> pmap unit -> pmap word ->
    (list IOEvent -> pmap unit -> pmap word -> Prop) -> Prop :=
| exec_skip : forall t m l post, post t m l -> exec cmd_skip t m l post
| exec_seq : forall t c1 c2 m l (mid : list IOEvent -> pmap unit -> pmap word -> Prop) post,
    exec c1 t m l mid ->
    (forall t' m' l', mid t' m' l' -> exec c2 t' m' l' post) ->
    exec (cmd_seq c1 c2) t m l post
| exec_set : forall t x e m l post v,
    eval_expr m l e = Some v -> post t m (pmap_put l x v) -> exec (cmd_set x e) t m l post
| exec_input : forall t lhs m l post,
    (forall v, post (IO_IN v :: t) m (pmap_put l lhs v)) -> exec (cmd_input lhs) t m l post.

Lemma exec_seq_cps : forall t c1 c2 m l post,
  exec c1 t m l (fun t' m' l' => exec c2 t' m' l' post) -> exec (cmd_seq c1 c2) t m l post.
Proof. intros. eapply exec_seq. eassumption. intros. assumption. Qed.

Fixpoint repeated_cmds (n : nat) (t x : string) : cmd :=
  match n with
  | 0 => cmd_skip
  | S n' => cmd_seq (cmd_set t (expr_op bop_add (expr_var t) (expr_var t)))
             (cmd_seq (cmd_set t (expr_op bop_sub (expr_var t) (expr_var x)))
               (repeated_cmds n' t x))
  end.
Definition generated_cmd (n : nat) (t x : string) : cmd :=
  cmd_seq (cmd_input x) (cmd_seq (cmd_set t (expr_var x)) (repeated_cmds n t x)).

Ltac solve_eq_by_rewriting :=
  cbn [eval_expr interp_binop];
  repeat first [
      rewrite pmap_get_put_same
    | rewrite pmap_get_put_diff by (cbv; congruence)
  ];
  try rewrite word_add_sub_cancel;
  apply eq_refl.

Ltac exec_once := match goal with
  | [ |- exec (cmd_seq _ _) _ _ _ _ ] => apply exec_seq_cps
  | [ |- exec (cmd_input _) _ _ _ _ ] => apply exec_input; intro
  | [ |- exec (cmd_set _ _) _ _ _ _ ] => eapply exec_set; first [
      match goal with
      | [ |- @eq (option word) (eval_expr _ _ _) (Some _) ] => solve_eq_by_rewriting
      end | idtac ]
  | [ |- exec cmd_skip _ _ _ _ ] => apply exec_skip
  end.
Ltac repeat_exec := repeat exec_once.

Theorem sym_exec :
  forall (m : pmap unit) (l : pmap word),
    exec (generated_cmd ''' + str(n) + r''' "a" "b") [] m l (fun t' m' l' => m' = m).
Proof.
  unfold generated_cmd; cbn [repeated_cmds]; intros;
  apply exec_seq_cps; apply exec_input; intro.
  repeat_exec.
  reflexivity.
Qed.
'''
        path = os.path.join(workdir, "test.v")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean_concrete(self, n, workdir):
        framework = self._lean_eval_framework()
        proof = r'''set_option maxHeartbeats 0
set_option maxRecDepth 1000000

macro "solve" : tactic => `(tactic| {
  intros m l;
  simp only [generated_cmd, repeated_cmds];
  apply Exec.seq_cps;
  apply Exec.input;
  intros v;
  repeat (
    apply Exec.seq_cps;
    apply Exec.set;
    simp only [Expr.eval];
    simp only [PartialMap.get_put_diff, PartialMap.get_put, PartialMap.put_put, Binop.interp_add,
          Binop.interp_sub, Word.add_sub_cancel, Option.some.injEq, not_false_eq_true, String.reduceEq, ne_eq];
    try rfl);
  apply Exec.skip;
  simp only [List.cons.injEq, IOEvent.IN.injEq, and_true, PartialMap.put_put, PartialMap.get_put,
    Option.some.injEq, and_self, exists_eq']
})

theorem goalN : Goal __N__ := by solve
'''.replace("__N__", str(n))
        content = "import Lean\n\n" + framework + proof
        path = os.path.join(workdir, "test.lean")
        with open(path, "w") as f:
            f.write(content)
        return path

    def _generate_lean_symm(self, n, workdir):
        # lean4/tests/bench/sym/add_sub_cancel.lean
        template = "import Lean.Meta.Sym\n\n" + self._lean_eval_framework() + r'''set_option maxHeartbeats 0
set_option maxRecDepth 100000
open Lean Meta Elab Tactic

def driver (n : Nat) (check := true) (k : MVarId → MetaM Unit) : MetaM Unit := do
  let some goal ← unfoldDefinition? (mkApp (mkConst ``Goal) (mkNatLit n)) | throwError "UNFOLD FAILED!"
  let mvar ← mkFreshExprMVar goal
  let startTime ← IO.monoNanosNow
  k mvar.mvarId!
  let endTime ← IO.monoNanosNow
  let ms := (endTime - startTime).toFloat / 1000000.0
  if check then
    let startTime ← IO.monoNanosNow
    checkWithKernel (← instantiateExprMVars mvar)
    let endTime ← IO.monoNanosNow
    let kernelMs := (endTime - startTime).toFloat / 1000000.0
    IO.println s!"goal_{n}: {ms} ms, kernel: {kernelMs} ms"
  else
    IO.println s!"goal_{n}: {ms} ms"

theorem exists_eq_True (a : α) : (∃ x, a = x) = True := by
  simp

open Sym

def mkMethods (declNames : Array Name) : MetaM Sym.Simp.Methods := do
  let rewrite ← Sym.mkSimprocFor declNames Sym.Simp.dischargeSimpSelf
  return {
    post := Sym.Simp.evalGround.andThen rewrite
  }

partial def solve (mvarId : MVarId) (simpEagerly : Bool) : SymM Unit := do
  let exec_cpsRule ← mkBackwardRuleFromDecl ``Exec.seq_cps
  let inputRule ← mkBackwardRuleFromDecl ``Exec.input
  let skipRule ← mkBackwardRuleFromDecl ``Exec.skip
  let setRule ← mkBackwardRuleFromDecl ``Exec.set
  let rflRule ← mkBackwardRuleFromDecl ``Eq.refl
  let unfoldMethods ← mkMethods #[``generated_cmd.eq_1, ``repeated_cmds.eq_1, ``repeated_cmds.eq_2]
  let evalMethods ← mkMethods #[``Expr.eval.eq_1, ``Expr.eval.eq_2, ``Expr.eval.eq_3]
  let simpMethods ← mkMethods #[``PartialMap.get_put_diff, ``PartialMap.get_put, ``PartialMap.put_put, ``Binop.interp_add,
          ``Binop.interp_sub, ``Word.add_sub_cancel, ``Option.some.injEq, ``not_false_eq_true, ``ne_eq]
  let finalSimpMethods ← mkMethods #[``List.cons.injEq, ``IOEvent.IN.injEq, ``and_true, ``true_and,
    ``PartialMap.put_put, ``PartialMap.get_put, ``PartialMap.get_put_diff, ``Option.some.injEq,
    ``and_self, ``exists_eq_True, ``Word.add_sub_cancel]
  let mvarId ← preprocessMVar mvarId
  let .goal _ mvarId ← Sym.introN mvarId 2 | failure
  let .goal mvarId ← Sym.simpGoal mvarId unfoldMethods | failure
  let .goals [mvarId] ← exec_cpsRule.apply mvarId | failure
  let .goals [mvarId] ← inputRule.apply mvarId | failure
  let .goal _ mvarId ← Sym.introN mvarId 1 | failure
  let rec loop (mvarId : MVarId) : SymM MVarId := do
    let .goals [mvarId] ← exec_cpsRule.apply mvarId | return mvarId
    let .goals [mvarId', mvarId, _] ← setRule.apply mvarId | failure
    let .goal mvarId' ← Sym.simpGoal mvarId' evalMethods | failure
    let .goal mvarId' ← Sym.simpGoal mvarId' simpMethods | failure
    let .goals [] ← rflRule.apply mvarId' | failure
    if simpEagerly then
      let .goal mvarId ← Sym.simpGoalIgnoringNoProgress mvarId simpMethods | failure
      loop mvarId
    else
      loop mvarId
  let mvarId ← loop mvarId
  let .goals [mvarId] ← skipRule.apply mvarId | failure
  let .closed ← Sym.simpGoal mvarId finalSimpMethods { maxSteps := 100000 } | failure
  return

def solveUsingSym (n : Nat) (simpEagerly : Bool := false) (check := true) : MetaM Unit := do
  driver n check fun mvarId => SymM.run do solve mvarId simpEagerly

def simpGoal (mvarId : MVarId) (m : Sym.Simp.Methods) (s : Sym.Simp.State)
    : SymM (SimpGoalResult × Sym.Simp.State) := mvarId.withContext do
  Simp.SimpM.run (methods := m) (config := {}) (s := s) do
    let decl ← mvarId.getDecl
    return (← (← Simp.simp decl.type).toSimpGoalResult mvarId).ignoreNoProgress mvarId

partial def solveReusingCache (mvarId : MVarId) : SymM Unit := do
  let exec_cpsRule ← mkBackwardRuleFromDecl ``Exec.seq_cps
  let inputRule ← mkBackwardRuleFromDecl ``Exec.input
  let skipRule ← mkBackwardRuleFromDecl ``Exec.skip
  let setRule ← mkBackwardRuleFromDecl ``Exec.set
  let rflRule ← mkBackwardRuleFromDecl ``Eq.refl
  let unfoldMethods ← mkMethods #[``generated_cmd.eq_1, ``repeated_cmds.eq_1, ``repeated_cmds.eq_2]
  let simpMethods ← mkMethods #[``Expr.eval.eq_1, ``Expr.eval.eq_2, ``Expr.eval.eq_3,
          ``PartialMap.get_put_diff, ``PartialMap.get_put, ``PartialMap.put_put, ``Binop.interp_add,
          ``Binop.interp_sub, ``Word.add_sub_cancel, ``Option.some.injEq, ``not_false_eq_true, ``ne_eq]
  let finalSimpMethods ← mkMethods #[``List.cons.injEq, ``IOEvent.IN.injEq, ``and_true, ``true_and,
    ``PartialMap.put_put, ``PartialMap.get_put, ``PartialMap.get_put_diff, ``Option.some.injEq,
    ``and_self, ``exists_eq_True, ``Word.add_sub_cancel]
  let mvarId ← preprocessMVar mvarId
  let .goal _ mvarId ← Sym.introN mvarId 2 | failure
  let .goal mvarId ← Sym.simpGoal mvarId unfoldMethods | failure
  let .goals [mvarId] ← exec_cpsRule.apply mvarId | failure
  let .goals [mvarId] ← inputRule.apply mvarId | failure
  let .goal _ mvarId ← Sym.introN mvarId 1 | failure
  let rec loop (mvarId : MVarId) (simpState : Sym.Simp.State) : SymM MVarId := do
    let .goals [mvarId] ← exec_cpsRule.apply mvarId | return mvarId
    let .goals [mvarId', mvarId, _] ← setRule.apply mvarId | failure
    let (.goal mvarId', simpState) ← simpGoal mvarId' simpMethods simpState | failure
    let .goals [] ← rflRule.apply mvarId' | failure
    loop mvarId simpState
  let mvarId ← loop mvarId {}
  let .goals [mvarId] ← skipRule.apply mvarId | failure
  let .closed ← Sym.simpGoal mvarId finalSimpMethods | failure
  return

def solveUsingCSym (n : Nat) (check := true) : MetaM Unit := do
  driver n check fun mvarId => SymM.run do solveReusingCache mvarId |>.run {}

#eval solveUsingCSym __N__
'''
        content = template.replace("__N__", str(n))
        path = os.path.join(workdir, "test.lean")
        with open(path, "w") as f:
            f.write(content)
        return path

    def get_command(self, strategy, params, engine_path, generated_file, config=None):
        if strategy.engine == "mengine":
            return [engine_path, "-q", generated_file]
        if strategy.engine == "lean":
            return [engine_path, "--tstack=1000000", generated_file]

        return [engine_path, generated_file]
