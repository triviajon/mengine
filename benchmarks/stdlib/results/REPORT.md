# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 62.3 ms (±12.1), MEngine 2.0 ms (±1.5). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 22 | 86.7 | 24.7 | 13.2 | 11.2 | 2.21× |
| `Lists` | 11 | 173.9 | 45.8 | 99.4 | 97.4 | 0.47× |
| `Logic` | 10 | 81.9 | 17.7 | 7.9 | 5.9 | 3.03× |
| `Nat` | 31 | 122.5 | 57.9 | 44.0 | 42.0 | 1.38× |
| `Peano` | 3 | 66.9 | ~0 | 2.7 | ~0 | — |

**Modules:** 5, 77 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 1.44× geomean (median 1.80×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
