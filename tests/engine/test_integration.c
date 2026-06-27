#include "src/common/options.h"
#include "src/runtime/runtime.h"
#include "tests/helpers/test_framework.h"

static MEngineRuntime *make_rt(void) {
    MEngineOptions opts = {0};
    opts.quiet = true;
    return mengine_runtime_new(&opts);
}

static void run_ok(const char *name, const char *src) {
    test_start(name);
    MEngineRuntime *rt = make_rt();
    int rc = mengine_runtime_exec_string(rt, src);
    mengine_runtime_free(rt);
    assert_equal_int(0, rc, "expected success");
}

static void run_fail(const char *name, const char *src) {
    test_start(name);
    MEngineRuntime *rt = make_rt();
    int rc = mengine_runtime_exec_string(rt, src);
    mengine_runtime_free(rt);
    assert_true(rc != 0, "expected failure");
}

/* ── axiom / definition / check ────────────────────────────────────────── */

static void test_axiom_and_check(void) {
    run_ok("axiom and check",
           "Axiom nat : Type.\n"
           "Axiom zero : nat.\n"
           "Check zero.\n");
}

static void test_definition(void) {
    run_ok("definition and check",
           "Axiom nat : Type.\n"
           "Definition id : forall (A : Type), forall (x : A), A :=\n"
           "  fun (A : Type) => fun (x : A) => x.\n"
           "Check id.\n");
}

/* ── inductive types ────────────────────────────────────────────────────── */

static void test_inductive_nat(void) {
    run_ok("inductive nat and constructors",
           "Inductive nat : Type := | O : nat | S : forall (_: nat), nat.\n"
           "Check nat.\n"
           "Check (S O).\n"
           "Check nat_ind.\n");
}

/* The generated induction principle must carry an induction hypothesis for each
 * recursive argument (not just case analysis).  This proof only type-checks if
 * `case_S` is `forall n, P n -> P (S n)`: `step` would not match a hypothesis-
 * free `forall n, P (S n)`. */
static void test_induction_principle_has_ih(void) {
    run_ok("induction principle carries the induction hypothesis",
           "Inductive nat : Type := | O : nat | S : forall (_: nat), nat.\n"
           "Axiom P : forall (_: nat), Prop.\n"
           "Axiom base : P O.\n"
           "Axiom step : forall (n : nat), forall (_: P n), P (S n).\n"
           "Theorem allP : forall (n : nat), P n.\n"
           "intro n.\n"
           "apply (nat_ind P).\n"
           "exact base.\n"
           "exact step.\n");
}

static void test_inductive_match_pred(void) {
    run_ok("match expression: predecessor",
           "Inductive nat : Type := | O : nat | S : forall (_: nat), nat.\n"
           "Definition pred : forall (_: nat), nat :=\n"
           "  fun (n : nat) => match n with | O => O | S m => m end.\n"
           "Check pred.\n");
}

static void test_inductive_bool(void) {
    run_ok("inductive bool and not",
           "Inductive bool : Type := | true : bool | false : bool.\n"
           "Definition not : forall (_: bool), bool :=\n"
           "  fun (b : bool) => match b with | true => false | false => true end.\n"
           "Check not.\n");
}

/* ── fixpoints ──────────────────────────────────────────────────────────── */

static void test_fixpoint_add(void) {
    run_ok("fixpoint add",
           "Inductive nat : Type := | O : nat | S : forall (_: nat), nat.\n"
           "Fixpoint add (n : nat) (m : nat) {struct n} : nat :=\n"
           "  match n with | O => m | S n' => S (add n' m) end.\n"
           "Check add.\n");
}

static void test_fixpoint_eval(void) {
    run_ok("fixpoint evaluation: add (S O) O",
           "Inductive nat : Type := | O : nat | S : forall (_: nat), nat.\n"
           "Fixpoint add (n : nat) (m : nat) {struct n} : nat :=\n"
           "  match n with | O => m | S n' => S (add n' m) end.\n"
           "Eval compute in (add O O).\n"
           "Eval compute in (add (S O) O).\n"
           "Eval compute in (add (S (S O)) (S O)).\n");
}

/* ── simple tactics ─────────────────────────────────────────────────────── */

static void test_tactic_exact(void) {
    run_ok("tactic exact",
           "Axiom P : Prop.\n"
           "Axiom hp : P.\n"
           "Theorem t : P.\n"
           "exact hp.\n");
}

static void test_tactic_intro_exact(void) {
    run_ok("tactic intro + exact",
           "Theorem t : forall (P : Prop), forall (x : P), P.\n"
           "intro P; intro x; exact x.\n");
}

static void test_tactic_assumption(void) {
    run_ok("tactic assumption",
           "Theorem t : forall (P : Prop), forall (x : P), P.\n"
           "intro P; intro x; assumption.\n");
}

static void test_tactic_reflexivity(void) {
    run_ok("tactic reflexivity",
           "Axiom A : Type.\n"
           "Axiom a : A.\n"
           "Theorem t : eq A a a.\n"
           "reflexivity.\n");
}

static void test_tactic_apply(void) {
    run_ok("tactic apply",
           "Axiom A : Prop.\n"
           "Axiom B : Prop.\n"
           "Axiom hab : forall (_: A), B.\n"
           "Axiom ha : A.\n"
           "Theorem t : B.\n"
           "apply hab; exact ha.\n");
}

static void test_nested_beta_intro_subst(void) {
    run_ok("nested beta substitution under intro",
           "Axiom X : Type.\n"
           "Axiom Y : Type.\n"
           "Axiom y : Y.\n"
           "Theorem t : forall (m : X),\n"
           "  (((fun (m2 : X) => fun (l2 : Y) => eq X m2 m) m) y).\n"
           "intro m.\n"
           "reflexivity.\n");
}

/* ── tactic combinators ─────────────────────────────────────────────────── */

static void test_tactic_first(void) {
    run_ok("tactic first",
           "Axiom P : Prop.\n"
           "Axiom hp : P.\n"
           "Theorem t : P.\n"
           "first [ exact hp ].\n");
}

static void test_tactic_first_fallback(void) {
    run_ok("tactic first with fallback",
           "Axiom P : Prop.\n"
           "Axiom hp : P.\n"
           "Axiom Q : Prop.\n"
           "Theorem t : P.\n"
           "first [ assumption | exact hp ].\n");
}

static void test_tactic_try(void) {
    run_ok("tactic try swallows failure",
           "Axiom P : Prop.\n"
           "Axiom hp : P.\n"
           "Theorem t : P.\n"
           "try apply hp; exact hp.\n");
}

static void test_tactic_repeat(void) {
    run_ok("tactic repeat intro + assumption",
           "Theorem t : forall (A : Prop), forall (B : Prop), forall (a : A), forall (b : B), A.\n"
           "repeat intro; assumption.\n");
}

static void test_tactic_orelse(void) {
    run_ok("tactic || (orelse)",
           "Axiom P : Prop.\n"
           "Axiom hp : P.\n"
           "Theorem t : P.\n"
           "assumption || exact hp.\n");
}

/* ── split / left / right / exists ─────────────────────────────────────── */

static void test_tactic_split(void) {
    run_ok("tactic split for conjunction",
           "Axiom P : Prop.\n"
           "Axiom Q : Prop.\n"
           "Axiom hp : P.\n"
           "Axiom hq : Q.\n"
           "Theorem t : and P Q.\n"
           "split; first [ exact hp | exact hq ].\n");
}

static void test_tactic_left(void) {
    run_ok("tactic left for disjunction",
           "Axiom P : Prop.\n"
           "Axiom Q : Prop.\n"
           "Axiom hp : P.\n"
           "Theorem t : or P Q.\n"
           "left; exact hp.\n");
}

static void test_tactic_right(void) {
    run_ok("tactic right for disjunction",
           "Axiom P : Prop.\n"
           "Axiom Q : Prop.\n"
           "Axiom hq : Q.\n"
           "Theorem t : or P Q.\n"
           "right; exact hq.\n");
}

static void test_tactic_exists(void) {
    run_ok("tactic exists",
           "Axiom N : Type.\n"
           "Axiom z : N.\n"
           "Axiom R : forall (_: N), Prop.\n"
           "Axiom hr : R z.\n"
           "Theorem t : ex N R.\n"
           "exists z.\n"
           "exact hr.\n");
}

/* ── rewriting ──────────────────────────────────────────────────────────── */

static void test_rewrite_simple(void) {
    run_ok("rewrite add_n_0 once",
           "Axiom nat : Type.\n"
           "Axiom zero : nat.\n"
           "Axiom v0 : nat.\n"
           "Axiom add : forall (_: nat), forall (_: nat), nat.\n"
           "Axiom add_n_0 : forall (n: nat), eq nat (add n zero) (n).\n"
           "Theorem t : eq nat (add v0 zero) v0.\n"
           "rewrite add_n_0 with eq.\n"
           "apply eq_refl.\n");
}

static void test_rewrite_tree(void) {
    run_ok("rewrite add_n_0 tree",
           "Axiom nat : Type.\n"
           "Axiom zero : nat.\n"
           "Axiom v0 : nat.\n"
           "Axiom add : forall (_: nat), forall (_: nat), nat.\n"
           "Axiom add_n_0 : forall (n: nat), eq nat (add n zero) (n).\n"
           "Theorem t : eq nat (add (add (add v0 zero) zero) zero) v0.\n"
           "rewrite add_n_0 with eq.\n"
           "apply eq_refl.\n");
}

static void test_rewrite_chained_mod(void) {
    run_ok("rewrite chained modulo",
           "Axiom nat : Type.\n"
           "Axiom mod : forall (_: nat), forall (_: nat), nat.\n"
           "Lemma mod_mod : forall (a: nat), forall (n: nat), eq nat (mod (mod a n) n) (mod a n).\n"
           "Admitted.\n"
           "Theorem t : forall (b: nat), forall (p: nat),\n"
           "  eq nat (mod (mod (mod (mod b p) p) p) p) (mod b p).\n"
           "intro b; intro p.\n"
           "rewrite mod_mod with eq.\n"
           "apply eq_refl.\n");
}

/* ── match Goal ─────────────────────────────────────────────────────────── */

static void test_match_goal_conclusion(void) {
    run_ok("match Goal on conclusion wildcard",
           "Axiom A : Prop.\n"
           "Axiom a : A.\n"
           "Theorem t : A.\n"
           "match Goal with | [ |- _ ] => exact a end.\n");
}

static void test_match_goal_hypothesis(void) {
    run_ok("match Goal hypothesis pattern",
           "Axiom A : Prop.\n"
           "Theorem t : forall (x : A), A.\n"
           "intro x.\n"
           "match Goal with | [ H : ?P |- ?P ] => exact H end.\n");
}

static void test_match_goal_multi_hyp(void) {
    run_ok("match Goal multiple hypotheses",
           "Axiom A : Prop.\n"
           "Axiom B : Prop.\n"
           "Theorem t : forall (x : A), forall (y : B), B.\n"
           "intro; intro.\n"
           "match Goal with | [ H1 : A , H2 : B |- B ] => exact H2 end.\n");
}

static void test_match_goal_patvar_as_term(void) {
    run_ok("match Goal pattern var used as term",
           "Axiom A : Prop.\n"
           "Axiom refl_lemma : forall (P : Prop), forall (p : P), P.\n"
           "Axiom a : A.\n"
           "Theorem t : A.\n"
           "match Goal with | [ |- ?T ] => exact (refl_lemma T a) end.\n");
}

static void test_match_goal_whnf_conclusion(void) {
    run_ok("match Goal normalizes conclusion to WHNF",
           "Axiom A : Prop.\n"
           "Axiom a : A.\n"
           "Theorem t : ((fun (P : Prop) => P) A).\n"
           "match Goal with | [ |- A ] => exact a end.\n");
}

/* ── named tactic definitions ───────────────────────────────────────────── */

static void test_named_tactic_zero_arg(void) {
    run_ok("named tactic zero args",
           "Axiom A : Prop.\n"
           "Axiom a : A.\n"
           "Tactic my_assumption := assumption.\n"
           "Theorem t : A.\n"
           "my_assumption.\n");
}

static void test_named_tactic_parameterized(void) {
    run_ok("named tactic with parameter",
           "Axiom A : Prop.\n"
           "Axiom a : A.\n"
           "Tactic my_exact x := exact x.\n"
           "Theorem t : A.\n"
           "my_exact a.\n");
}

static void test_named_tactic_composed(void) {
    run_ok("named tactic using another named tactic",
           "Axiom A : Prop.\n"
           "Tactic find_hyp := match Goal with | [ H : ?P |- ?P ] => exact H end.\n"
           "Tactic auto_solve := repeat intro; find_hyp.\n"
           "Theorem t : forall (x : A), A.\n"
           "auto_solve.\n");
}

/* ── let bindings ───────────────────────────────────────────────────────── */

static void test_let_in_term(void) {
    run_ok("let-in term in theorem",
           "Axiom nat : Type.\n"
           "Axiom zero : nat.\n"
           "Axiom v0 : nat.\n"
           "Axiom add : forall (_: nat), forall (_: nat), nat.\n"
           "Axiom add_n_0 : forall (n: nat), eq nat (add n zero) n.\n"
           "Theorem t : eq nat\n"
           "  (let v1 : nat := add v0 zero in v1) v0.\n"
           "rewrite add_n_0 with eq.\n"
           "apply eq_refl.\n");
}

/* ── forall proof ───────────────────────────────────────────────────────── */

static void test_forall_proof_chain(void) {
    run_ok("forall proof with intro chain",
           "Theorem t : forall (A : Prop), forall (B : forall (_: A), Prop),\n"
           "  forall (a : A), forall (b : B a), B a.\n"
           "intro A; intro B; intro a; intro b; exact b.\n");
}

/* ── error cases ────────────────────────────────────────────────────────── */

static void test_type_error_fails(void) {
    run_fail("type error is rejected",
             "Axiom nat : Type.\n"
             "Axiom zero : nat.\n"
             "Definition bad : nat := Type.\n");
}

static void test_unfinished_proof_fails(void) {
    run_fail("unfinished proof is rejected",
             "Axiom P : Prop.\n"
             "Theorem t : P.\n"
             "intro.\n");
}

/* ── parametric inductive ───────────────────────────────────────────────── */

static void test_parametric_list(void) {
    run_ok("sep_list-style parametric inductive",
           "Inductive list (A : Type) : Type :=\n"
           "  | nil : list A\n"
           "  | cons : forall (_: A), forall (_: list A), list A.\n"
           "Check list.\n"
           "Check nil.\n"
           "Check cons.\n");
}

/* Parametric inductives also get an induction principle (parameters bound once,
 * out front), with an induction hypothesis on the recursive argument. This proof
 * only type-checks if list_ind has the shape
 *   forall A P, P (nil A) -> (forall a l, P l -> P (cons A a l)) -> forall l, P l. */
static void test_parametric_induction_principle(void) {
    run_ok("parametric induction principle is usable",
           "Inductive list (A : Type) : Type :=\n"
           "  | nil : list A\n"
           "  | cons : forall (_: A), forall (_: list A), list A.\n"
           "Axiom A : Type.\n"
           "Axiom P : forall (_: list A), Prop.\n"
           "Axiom base : P (nil A).\n"
           "Axiom step : forall (a : A), forall (l : list A), forall (_: P l), P (cons A a l).\n"
           "Theorem allL : forall (l : list A), P l.\n"
           "intro l.\n"
           "apply (list_ind A P).\n"
           "exact base.\n"
           "exact step.\n");
}

void run_integration_tests(void) {
    test_suite_start("Integration Tests");

    test_axiom_and_check();
    test_definition();
    test_inductive_nat();
    test_induction_principle_has_ih();
    test_parametric_induction_principle();
    test_inductive_match_pred();
    test_inductive_bool();
    test_fixpoint_add();
    test_fixpoint_eval();
    test_tactic_exact();
    test_tactic_intro_exact();
    test_tactic_assumption();
    test_tactic_reflexivity();
    test_tactic_apply();
    test_nested_beta_intro_subst();
    test_tactic_first();
    test_tactic_first_fallback();
    test_tactic_try();
    test_tactic_repeat();
    test_tactic_orelse();
    test_tactic_split();
    test_tactic_left();
    test_tactic_right();
    test_tactic_exists();
    test_rewrite_simple();
    test_rewrite_tree();
    test_rewrite_chained_mod();
    test_match_goal_conclusion();
    test_match_goal_hypothesis();
    test_match_goal_multi_hyp();
    test_match_goal_patvar_as_term();
    test_match_goal_whnf_conclusion();
    test_named_tactic_zero_arg();
    test_named_tactic_parameterized();
    test_named_tactic_composed();
    test_let_in_term();
    test_forall_proof_chain();
    test_type_error_fails();
    test_unfinished_proof_fails();
    test_parametric_list();

    test_suite_end();
}
