# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 66.7 ms (±5.0), MEngine 1.8 ms (±1.4). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 24 | 105.0 | 38.3 | 7.3 | 5.5 | 6.97× |
| `Lists` | 5 | 81.7 | 15.1 | 26.8 | 25.1 | 0.60× |
| `Logic` | 14 | 83.7 | 17.0 | 6.5 | 4.7 | 3.61× |
| `Nat` | 27 | 127.2 | 60.6 | 37.7 | 35.9 | 1.69× |
| `Peano` | 5 | 69.1 | ~0 | 2.0 | ~0 | — |

**Modules:** 5, 75 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 2.25× geomean (median 3.61×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
