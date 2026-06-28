# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 62.3 ms (±15.7), MEngine 4.0 ms (±0.2). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Unit | Category | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|------|----------|-----------------|-----------------|--------------------|--------------------|---------------|
| `bool_andb_b_b` | bool | 65.4 | ~0 | 2.3 | ~0 | — |
| `bool_andb_false_l` | bool | 65.3 | ~0 | 2.0 | ~0 | — |
| `bool_andb_true_l` | bool | 63.6 | ~0 | 3.0 | ~0 | — |
| `bool_andb_true_r` | bool | 64.2 | ~0 | 2.2 | ~0 | — |
| `bool_negb_false` | bool | 63.8 | ~0 | 1.9 | ~0 | — |
| `bool_negb_involutive` | bool | 66.1 | ~0 | 2.2 | ~0 | — |
| `bool_negb_true` | bool | 81.0 | 18.7 | 2.1 | ~0 | — |
| `bool_orb_b_b` | bool | 64.5 | ~0 | 2.3 | ~0 | — |
| `bool_orb_false_l` | bool | 66.5 | ~0 | 2.3 | ~0 | — |
| `bool_orb_false_r` | bool | 68.5 | ~0 | 2.2 | ~0 | — |
| `bool_orb_true_l` | bool | 63.3 | ~0 | 1.9 | ~0 | — |
| `eq_f_equal` | eq | 65.4 | ~0 | 2.4 | ~0 | — |
| `eq_f_equal2` | eq | 65.6 | ~0 | 3.1 | ~0 | — |
| `eq_refl_x` | eq | 64.7 | ~0 | 1.9 | ~0 | — |
| `eq_sym` | eq | 66.3 | ~0 | 2.0 | ~0 | — |
| `eq_trans` | eq | 64.5 | ~0 | 2.1 | ~0 | — |
| `ex_eq` | ex | 65.2 | ~0 | 2.0 | ~0 | — |
| `ex_intro` | ex | 66.4 | ~0 | 2.1 | ~0 | — |
| `le_0_n` | le | 86.8 | 24.5 | 2.3 | ~0 | — |
| `le_1_2` | le | 65.5 | ~0 | 2.2 | ~0 | — |
| `le_2_4` | le | 67.7 | ~0 | 1.9 | ~0 | — |
| `le_refl_n` | le | 64.4 | ~0 | 1.9 | ~0 | — |
| `logic_and_intro` | logic | 64.5 | ~0 | 1.8 | ~0 | — |
| `logic_and_intro3` | logic | 64.9 | ~0 | 4.6 | 0.6 | — |
| `logic_imp_refl` | logic | 64.2 | ~0 | 1.8 | ~0 | — |
| `logic_imp_trans` | logic | 65.1 | ~0 | 2.2 | ~0 | — |
| `logic_or_introl` | logic | 64.9 | ~0 | 1.9 | ~0 | — |
| `logic_or_intror` | logic | 64.1 | ~0 | 2.1 | ~0 | — |
| `nat_add_0_l` | nat | 64.5 | ~0 | 1.9 | ~0 | — |
| `nat_add_2_2` | nat | 63.9 | ~0 | 2.0 | ~0 | — |
| `nat_add_3_4` | nat | 64.5 | ~0 | 2.0 | ~0 | — |
| `nat_mul_2_3` | nat | 63.8 | ~0 | 2.4 | ~0 | — |
| `nat_sub_5_2` | nat | 64.3 | ~0 | 2.1 | ~0 | — |

**Units:** 33 (all Tier A).
**Below startup-noise floor (proof time ~0 on either engine):** 33 of 33.

> For this corpus the per-unit proofs are trivial: their startup-subtracted cost is at or below measurement jitter for both engines, so the headline whole-file ratio (~30×) is really a *process-startup* ratio, not a proof-speed ratio. Heavier units are needed to measure proof speed above the noise floor.
