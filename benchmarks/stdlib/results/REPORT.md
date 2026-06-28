# Rocq stdlib benchmark — MEngine vs Rocq

Per-unit wall-clock (best of N trials), whole-process: each engine pays its own startup (MEngine loads `prelude/tactics.me` + the compat prelude; Rocq starts its process and loads `Coq.Init`).

| Unit | Category | Rocq (ms) | MEngine (ms) | Speedup (Rocq/MEngine) |
|------|----------|-----------|--------------|------------------------|
| `bool_andb_b_b` | bool | 66.2 | 2.2 | 30.45× |
| `bool_andb_false_l` | bool | 65.4 | 1.9 | 34.10× |
| `bool_andb_true_l` | bool | 64.5 | 2.0 | 32.07× |
| `bool_andb_true_r` | bool | 65.1 | 2.1 | 31.40× |
| `bool_negb_false` | bool | 63.7 | 2.0 | 31.63× |
| `bool_negb_involutive` | bool | 66.7 | 2.2 | 30.67× |
| `bool_negb_true` | bool | 64.0 | 2.0 | 32.43× |
| `bool_orb_b_b` | bool | 66.2 | 2.1 | 31.09× |
| `bool_orb_false_l` | bool | 65.4 | 2.1 | 31.31× |
| `bool_orb_false_r` | bool | 65.8 | 2.0 | 33.04× |
| `bool_orb_true_l` | bool | 63.4 | 2.0 | 31.49× |
| `le_0_n` | le | 63.3 | 1.9 | 33.32× |
| `le_1_2` | le | 67.9 | 2.1 | 32.68× |
| `le_2_4` | le | 65.2 | 2.0 | 32.10× |
| `le_refl_n` | le | 68.7 | 2.0 | 35.01× |
| `logic_and_intro` | logic | 65.4 | 2.0 | 33.48× |
| `logic_and_intro3` | logic | 67.9 | 1.9 | 36.21× |
| `logic_imp_refl` | logic | 65.3 | 2.4 | 27.48× |
| `logic_imp_trans` | logic | 65.2 | 1.8 | 36.12× |
| `logic_or_introl` | logic | 64.6 | 2.1 | 30.17× |
| `logic_or_intror` | logic | 67.0 | 1.9 | 35.86× |
| `nat_add_0_l` | nat | 67.4 | 1.8 | 36.51× |
| `nat_add_2_2` | nat | 65.4 | 2.0 | 32.65× |
| `nat_add_3_4` | nat | 75.8 | 2.1 | 36.39× |
| `nat_mul_2_3` | nat | 66.3 | 2.7 | 24.63× |
| `nat_sub_5_2` | nat | 66.7 | 2.3 | 29.27× |

**Units:** 26 (all Tier A).  **Both-succeed:** 26.
**Geometric-mean speedup (Rocq/MEngine):** 32.25×  (median 32.43×).

> MEngine times are dominated by prelude+compat startup at this problem size; the comparison reflects fixed per-invocation cost, not asymptotics. See README for scope and caveats.
