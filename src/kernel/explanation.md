Read [](file:///home/jonros/mengine/src/kernel/bottomup_subst.c#1-1), lines 1 to 320

Read [](file:///home/jonros/mengine/src/kernel/bottomup_subst.c#320-320), lines 320 to 600

Here's the full algorithm explained top to bottom.

---

## Overview

The goal is: given a term `t` and a parallel substitution `{x₁ → a₁, ..., xₙ → aₙ}`, compute `t[x₁:=a₁, ..., xₙ:=aₙ]` efficiently.

The traditional approach (`topdown_subst.c`) traverses the entire tree — O(|t|). The bottomup approach exploits the fact that expressions form a **shared DAG with uplinks** (every node knows all its parents). It only touches the nodes that actually contain a substitution target, giving O(spine) cost where "spine" is the set of ancestors of any target within `t`.

---

## Data Structures

**Expression DAG**: Nodes are shared (`Expression *` identity = value identity). Every child → parent reference is tracked in `uplinks` (a doubly-linked list of `Uplink` records). Each binder sub-struct also stores a `DLLNode *` back-pointer (e.g. `func_uplink_node`, `body_uplink_node`) for O(1) removal of a specific uplink.

**Substitution map**: Two parallel `DoublyLinkedList *` (`old_exprs` / `new_exprs`) that are stack-like — entries for binder variables are pushed/popped as the rebuild recurses through binders. `subst_map_lookup` scans linearly (usually tiny).

**`visit_gen`**: A `uint64_t` field on every `Expression`. Used as a generation stamp to determine whether a node belongs to `t`'s structural subtree.

---

## Step 0 — Generation Stamp (`stamp_subtree`)

Before anything else, `stamp_subtree(t)` increments a global counter `g_subst_gen` and does a **DFS down from `t`**, setting every structural descendant's `visit_gen = gen`.

**Why**: The uplink graph is global — an axiom's bound variable has `FORALL_BOUND_VAR` uplinks pointing to *every* expression that uses that axiom. Without this filter, Phase 1's BFS upward would escape `t` and mark unrelated nodes. The stamp makes those escapees invisible: the BFS only enqueues parents where `parent->visit_gen == subtree_gen`.

**Critical detail**: The DFS must stamp not only structural children (func, arg, bound_var, body) but also the **types of bound variables** (`get_expression_type(bound_variable)`). Substitution targets can appear in binder types (e.g. `forall (_: eq nat x y), body`), and those type-nodes carry uplinks that the BFS must be able to follow.

---

## Phase 1 — Mark (`build_marked_set` / `mark_spine_from`)

For each substitution target `xᵢ`, do a **BFS upward through uplinks** from `xᵢ` to `t`, recording every node visited in a `marked` map.

```
mark_spine_from(xᵢ, t, marked, skip_ownership=0, subtree_gen)
```

The BFS filter: only enqueue `parent` if `parent->visit_gen == subtree_gen`. This confines the BFS to `t`'s subtree.

**`skip_ownership`**: The initial Phase 1 call uses `skip_ownership=0` — all uplink types are followed, including `FORALL_BOUND_VAR`. This is needed so that if a substitution target appears as a *type* of a binder's bound variable, the binder node itself gets marked. Later, when `spine_rebuild` freshens a binder variable and needs to re-mark inside the body, it calls `mark_spine_from` with `skip_ownership=1` to prevent the BFS from escaping upward through the binder boundary.

At the end of Phase 1, `marked` contains exactly the nodes that need to be path-copied: the ancestors of any substitution target within `t`.

---

## Phase 2 — Rebuild (`spine_rebuild`)

A top-down recursive walk of `t`. At each node, four cases:

**Case 1 — Direct hit**: `node ∈ old_exprs` → return the corresponding `new_exprs` entry immediately.

**Case 2 — Unmarked (but check stale context)**: If `node ∉ marked`, it's untouched — but there's a subtlety. After a binder is freshened (e.g. `λ x : T` becomes `λ x' : T'`), descendants that have `context = x` now have a **stale context pointer** (pointing to the old `x`). Even if they don't syntactically contain `x`, they must be rebuilt so the new node lives in the `x'` context chain. The stale-context check walks the context chain looking for any `old_exprs` entry; if found, marks and rebuilds.

**Case 3 — Memoised**: `node ∈ memo` → return cached result. The memo is scoped per binder level (a fresh `inner_memo` is allocated when entering a binder body and freed on exit), so sharing within the same binder scope is exploited but cross-scope results are not confused.

**Case 4 — Marked, not memoised**: Path-copy this node. The logic per tag:

- **`APP`**: Recursively rebuild `func` and `arg` via `maybe_rebuild` (which only recurses into marked or stale children). Remove old uplinks from the dead parent (`remove_uplink_by_node`, NULLing back-pointers immediately for reentrancy safety). Create a new APP with `init_app_expression_wc`. If the result is a beta-redex (LAMBDA applied to arg), `beta_reduce` is called immediately.

- **`LAMBDA` / `FORALL`**: Three sub-steps:
  1. Rebuild the bound-variable type via `maybe_rebuild`.
  2. **Freshen the bound variable**: create `x_bv2 = init_var_expression_wc(name, new_type, apps_ctx)`. If type and context are unchanged, reuse the original `x_bv` (hash-consing would return the same pointer anyway, but avoiding an extra `init_var` call preserves sharing and prevents the stale-context check from firing on the entire body).
  3. If `x_bv2 ≠ x_bv`: push `x_bv → x_bv2` onto the substitution map and call `mark_spine_from(x_bv, body, skip_ownership=1)` to mark the body's spine under the new mapping.
  4. Rebuild `body` with a **fresh `inner_memo`** (scope reset for the body).
  5. Pop `x_bv → x_bv2` off the substitution map.
  6. Remove stale uplinks, construct with `init_lambda_expression_wc(x_bv2, body2)`.

- **`MATCH`**: For each branch: rebuild the constructor, freshen each pattern variable (same fresh-var logic), re-mark body spine for changed pattern vars, rebuild body. Stale uplinks removed eagerly.

- **`FIX`**: Freshen `rec_var` and each `arg[i]` similarly; rebuild body.

**`apps_ctx`**: One key design choice — `apps_ctx` is the **outermost context** and does not change as `spine_rebuild` crosses binders. This means freshened bound variables all live in the outermost context. This is correct because the algorithm does not track per-scope context; the stale-context check (`context` field walk) handles propagation. Using a single stable `apps_ctx` avoids bugs where a rebuilt child's context wouldn't match the expected binder scope.

**`maybe_rebuild`**: A helper that wraps `spine_rebuild`. It skips the call entirely if `child ∉ marked` and the child has no stale context — this is the O(spine) optimization. Unmarked non-stale children are returned as-is.

---

## Reentrancy Guard (`g_uplink_subst_depth` / `_simple_topdown_psubst`)

The path `spine_rebuild(APP)` → `init_app_expression_wc` → `_construct_app_type` → `new_subst` → `_uplink_p_subst` is reentrant. An inner call would increment `g_subst_gen`, re-stamp nodes with a new generation, and invalidate the outer call's `subtree_gen` filter — causing missed marks and a segfault.

The fix: a static counter `g_uplink_subst_depth`. On entry to `_uplink_p_subst`:
- increment counter
- if `> 1`: dispatch to `_simple_topdown_psubst` (a plain structural recursion with no stamps/BFS/uplinks), return, decrement counter

`_simple_topdown_psubst` handles APP/LAMBDA/FORALL via direct recursion, threading `(Context *)x_bv2` into binder bodies (mirrors `topdown_subst.c`). FIX and MATCH are not expected in type-computation positions. The inner call owns its own fresh `old_exprs/new_exprs` lists so the outer call's lists are never touched.

---

## Complexity

| Phase | Cost |
|---|---|
| `stamp_subtree` | O(\|t\|) — visits every node once |
| Phase 1 mark | O(k · h) where k = number of substitution targets, h = max path length to root |
| Phase 2 rebuild | O(\|spine\|) — only visits marked nodes |

For typical proof-engine use (substituting a small number of variables in terms whose free occurrences are sparse), this is dramatically faster than the O(\|t\|) topdown approach — which is why test.me runs in 0.075s under bottomup vs ~1s under topdown.