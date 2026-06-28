# Rocq stdlib benchmark — MEngine vs Rocq

Per-unit wall-clock (best of N trials), whole-process: each engine pays its own startup (MEngine loads `prelude/tactics.me` + the compat prelude; Rocq starts its process and loads `Coq.Init`).

| Unit | Category | Rocq (ms) | MEngine (ms) | Speedup (Rocq/MEngine) |
|------|----------|-----------|--------------|------------------------|
| `bool_andb_b_b` | bool | 65.5 | 4.6 | 14.12× |
| `bool_andb_false_l` | bool | 65.5 | 2.1 | 31.58× |
| `bool_andb_true_l` | bool | 64.3 | 2.2 | 28.96× |
| `bool_andb_true_r` | bool | 65.5 | 2.1 | 31.57× |
| `bool_negb_false` | bool | 64.1 | 1.9 | 33.55× |
| `bool_negb_involutive` | bool | 66.6 | 2.3 | 29.31× |
| `bool_negb_true` | bool | 64.8 | 2.1 | 30.37× |
| `bool_orb_b_b` | bool | 69.1 | 2.1 | 32.86× |
| `bool_orb_false_l` | bool | 66.4 | 2.1 | 31.19× |
| `bool_orb_false_r` | bool | 65.7 | 2.3 | 28.70× |
| `bool_orb_true_l` | bool | 64.4 | 2.0 | 32.05× |
| `eq_f_equal` | eq | 67.2 | 2.3 | 29.19× |
| `eq_f_equal2` | eq | 66.6 | 3.3 | 20.27× |
| `eq_refl_x` | eq | 65.7 | 1.9 | 34.36× |
| `eq_sym` | eq | 69.1 | 2.2 | 31.59× |
| `eq_trans` | eq | 65.6 | 2.0 | 32.27× |
| `ex_eq` | ex | 76.5 | 2.1 | 37.19× |
| `ex_intro` | ex | 68.3 | 2.0 | 33.81× |
| `le_0_n` | le | 66.3 | 2.1 | 31.40× |
| `le_1_2` | le | 63.5 | 1.9 | 33.51× |
| `le_2_4` | le | 68.4 | 2.0 | 34.94× |
| `le_refl_n` | le | 66.0 | 2.3 | 28.92× |
| `logic_and_intro` | logic | 65.9 | 2.1 | 30.89× |
| `logic_and_intro3` | logic | 66.4 | 2.1 | 31.72× |
| `logic_imp_refl` | logic | 64.0 | 1.8 | 35.28× |
| `logic_imp_trans` | logic | 65.9 | 1.9 | 34.09× |
| `logic_or_introl` | logic | 64.5 | 2.0 | 32.67× |
| `logic_or_intror` | logic | 66.5 | 2.0 | 33.03× |
| `nat_add_0_l` | nat | 74.3 | 1.9 | 38.82× |
| `nat_add_2_2` | nat | 66.2 | 2.5 | 26.87× |
| `nat_add_3_4` | nat | 65.1 | 2.1 | 31.06× |
| `nat_mul_2_3` | nat | 63.6 | 2.4 | 26.33× |
| `nat_sub_5_2` | nat | 65.0 | 2.3 | 27.97× |

**Units:** 33 (all Tier A).  **Both-succeed:** 33.
**Geometric-mean speedup (Rocq/MEngine):** 30.51×  (median 31.58×).

> MEngine times are dominated by prelude+compat startup at this problem size; the comparison reflects fixed per-invocation cost, not asymptotics. See README for scope and caveats.
