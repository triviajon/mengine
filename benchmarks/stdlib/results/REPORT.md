# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. Every number below is the **best (minimum) of N trials**; **proof** columns are that whole-file minimum minus the startup-floor minimum, clamped at 0.

**Startup floor** (subtracted from each whole-file time): MEngine 2.1 ms (±1.4), one global floor; Rocq 61.0 ms (±10.0) for the `Require`-free modules, but **each module subtracts its own Require/Import floor** (Lists' is ~2× larger). The ± is that floor's trial standard deviation; a proof at or below its own floor's ± is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 22 | 87.9 | 27.1 | 6.6 | 4.6 | 5.91× |
| `Lists` | 11 | 170.9 | 44.3 | 100.4 | 98.4 | 0.45× |
| `Logic` | 10 | 78.3 | 15.7 | 7.3 | 5.2 | 3.00× |
| `Nat` | 31 | 128.9 | 66.2 | 43.5 | 41.4 | 1.60× |
| `Peano` | 3 | 66.1 | ~0 | 2.2 | ~0 | — |

**Modules:** 5, 77 lemmas — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 1.89× geomean (median 2.30×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
