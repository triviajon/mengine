# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 62.4 ms (±7.6), MEngine 2.0 ms (±0.4). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 24 | 91.3 | 29.4 | 6.6 | 4.6 | 6.36× |
| `Lists` | 11 | 175.8 | 47.6 | 104.3 | 102.3 | 0.47× |
| `Logic` | 14 | 82.5 | 18.8 | 7.5 | 5.5 | 3.44× |
| `Nat` | 34 | 131.2 | 67.7 | 48.1 | 46.1 | 1.47× |
| `Peano` | 5 | 69.4 | 6.2 | 2.5 | 0.5 | 11.64× |

**Modules:** 5, 88 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 0 of 5.
**Proof-only speedup (Rocq/MEngine), over the 5 module(s) above the noise floor:** 2.81× geomean (median 3.44×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
