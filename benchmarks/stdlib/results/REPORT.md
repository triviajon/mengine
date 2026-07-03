# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. Every number below is the **best (minimum) of N trials**; **proof** columns are that whole-file minimum minus the startup-floor minimum, clamped at 0.

**Startup floor:** Rocq 60.9 ms (±12.3), MEngine 2.0 ms (±1.0). The ± is the standard deviation of the floor's trials; a proof at or below its engine's ± is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 22 | 87.4 | 25.1 | 7.2 | 5.2 | 4.83× |
| `Lists` | 11 | 176.1 | 49.8 | 100.4 | 98.4 | 0.51× |
| `Logic` | 10 | 77.3 | 15.0 | 7.3 | 5.2 | 2.88× |
| `Nat` | 31 | 127.3 | 65.0 | 46.6 | 44.6 | 1.46× |
| `Peano` | 3 | 67.0 | ~0 | 2.3 | ~0 | — |

**Modules:** 5, 77 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 1.79× geomean (median 2.17×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
