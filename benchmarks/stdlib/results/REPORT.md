# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. Every number below is the **best (minimum) of N trials**; **proof** columns are that whole-file minimum minus the startup-floor minimum, clamped at 0.

**Startup floor** (subtracted from each whole-file time): MEngine 2.0 ms (±0.9), one global floor; Rocq 62.3 ms (±8.0) for the `Require`-free modules, but **each module subtracts its own Require/Import floor** (Lists' is ~2× larger). The ± is that floor's trial standard deviation; a proof at or below its own floor's ± is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 22 | 88.3 | 25.7 | 6.7 | 4.7 | 5.41× |
| `Lists` | 11 | 174.2 | 45.5 | 105.9 | 103.9 | 0.44× |
| `Logic` | 10 | 77.8 | 16.1 | 7.3 | 5.4 | 2.99× |
| `Nat` | 31 | 126.7 | 63.3 | 44.2 | 42.3 | 1.50× |
| `Peano` | 3 | 66.5 | ~0 | 2.3 | ~0 | — |

**Modules:** 5, 77 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 1.81× geomean (median 2.24×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
