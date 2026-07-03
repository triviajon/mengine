# Why `destr_bool` / `destruct_all` are not implementable as MEngine tactics

The stdlib proves the `Bool` lemmas with the `destr_bool` Ltac macro, which the
corpus cannot reuse verbatim. This note records *why* that macro — and the
`destruct_all` it is built on — cannot be written as a faithful MEngine tactic in
the current tactic language, so the gap is auditable in one place.

## What the macro is

In Rocq (`Coq.Bool.Bool` / `Coq.Init.Tactics`):

```coq
Ltac destruct_all t :=
  match goal with
  | x : t |- _ => destruct x; destruct_all t
  | _ => idtac
  end.

Ltac destr_bool :=
  intros; destruct_all bool; simpl in *; trivial; try discriminate.
```

So `destr_bool` is: intro everything, then `destruct` *every hypothesis of type
`bool` in the context*, simplify, and close. The whole construct is just
sequencing (`;`) plus `match goal` plus recursion — all of which MEngine has.
The load-bearing leaf is **`destruct x` on a hypothesis `x : bool`**, and that
is what MEngine cannot express.

## The obstacles

### 1. (Primary) No goal-abstraction primitive — the motive cannot be synthesised

`destruct x` on a context variable `x : bool` with goal `G` must build the proof
term `bool_ind (fun b => G[x:=b]) ?true ?false x`, i.e. it must **abstract `G`
over `x`** to form the eliminator's motive `fun b => G[x:=b]`.

MEngine's tactic-value language (keywords in
`src/common/token_keywords.def`) offers term *substitution* but no term
*abstraction*:

- `subst new body old` → `body[old := new]` (substitute a variable),
- `pair`/`fst`/`snd`, `app_func`/`app_arg` (decompose an application),
  `expr_eq`, `constr` (build a literal term).

There is **no** primitive that turns `(var, body)` into `λvar. body` (the inverse
of `subst`), and none that turns a `forall x, body` into the corresponding
`λx. body`. The only lambdas that ever enter a proof come from `intro` (a proof
term) or are written **literally in source** and lowered by `constr`/`exact`.

The corpus only gets away with case analysis because it writes the motive out in
source, where *name shadowing* performs the abstraction:

```
apply (bool_ind (fun (b : bool) => (((eq bool) ((andb b) true)) b))).
```

A generic tactic cannot do this. When a tactic matches `[ x : bool |- ?G ]`, the
goal `G` is bound as a **spliced `Expression`** (a pointer-unique kernel
variable), not as re-parseable source text, so re-binding the name does nothing:

```
Tactic destr_one := match Goal with
| [ b : bool |- ?G ] =>
    let motive := constr (fun (b : bool) => G) in   (* b here is a FRESH binder *)
    apply (bool_ind motive)                          (* motive = fun _ => G, a constant! *)
end.
```

The lambda's `b` and the spliced `G`'s `b` are different pointers, so the motive
degenerates to the **constant** function `fun _ => G`; `apply (bool_ind motive)`
then fails (`motive true = motive false = G`, but `forall x, motive x` does not
unify with `G`). Empirically this branch fails ("No branch matched in match
Goal"), while the literal-motive form above proves the identical goal.

### 2. (Primary) `eunify` does no higher-order motive inference

The obvious workaround — leave the motive as a hole and let unification infer it —
does not work either. `apply bool_ind` against a `forall (b:bool), P b` goal (or
an introduced `b : bool` goal) has to solve `?M b =?= G` for the motive `?M`;
MEngine's `eunify` does not solve this higher-order / Miller-pattern problem.
Empirically `apply bool_ind` (no explicit motive) fails (`fill: hole fill
failed`). This is exactly why the corpus must spell the motive out.

Obstacles 1 and 2 together are the real blocker: the motive can be supplied
*only* by writing it literally in the source, which a tactic that receives its
goal as a spliced term cannot do.

### 3. (Secondary) No hypothesis clearing — `destruct_all`'s recursion would not terminate

`destruct_all t` recurses on `match goal with x : t |- _ => destruct x;
destruct_all t`. In Rocq it terminates because `destruct x` **removes** `x : bool`
from the context (replacing it with the constructor in each branch), so the next
`match goal` no longer finds a `bool` hypothesis. MEngine has no clear /
substitute-in-context operation: applying `bool_ind` leaves the original
hypothesis `x` in scope, so the `match goal` loop would re-match the same
variable forever. Even with a motive-abstraction primitive, `destruct_all` would
need context clearing to terminate.

### 4. (Secondary) No `discriminate`

The trailing `try discriminate` has no MEngine primitive — `discriminate` needs
constructor no-confusion (eliminating a `true = false` hypothesis into `False`),
which is not a tactic-language one-liner. It is **vacuous for this corpus**
(every `bool` case closes by `reflexivity` after `simpl`), and it is under `try`,
so it does not affect provability here — but a faithful `destr_bool` would still
have to provide it.

## Consequence

A `destr_bool` that **takes the motive as an argument** (`destr_bool (fun (b:bool)
=> …)`) *is* writable, but it is strictly worse than the status quo: it still
forces the goal to be spelled out and is *more* verbose than Rocq's nullary
`destr_bool`, so it improves nothing about fidelity. The current corpus rendering
(`apply (bool_ind <motive>); simpl; reflexivity; …`) is already the most faithful
form available, which is why these lemmas are classified `untranslatable` rather
than re-proved with a macro.

## What would unlock it (engine change, not a tactic-script change)

The blocker is a missing *primitive*, not missing sequencing. Exposing an
abstraction primitive — e.g. `abstract <var> <body>` → `λvar. body`, wrapping the
existing `kernel_lambda_create` that `intro_step` already uses
(`src/tacticlanguage/tactic_interp.c`) — would let a tactic synthesise the
`bool_ind` motive and resolve obstacles 1–2. A faithful `destruct_all`/`destr_bool`
on top would additionally need context clearing (obstacle 3) and a `discriminate`
(obstacle 4). These touch the kernel/interp trusted surface, so they are out of
scope for `prelude/tactics.me` and have not been made.
