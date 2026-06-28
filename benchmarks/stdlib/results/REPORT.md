# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 68.3 ms (±6.0), MEngine 1.8 ms (±1.3). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 24 | 96.7 | 32.5 | 7.6 | 5.8 | 5.64× |
| `Lists` | 9 | 178.8 | 41.1 | 76.7 | 74.8 | 0.55× |
| `Logic` | 14 | 84.6 | 20.8 | 8.4 | 6.6 | 3.16× |
| `Nat` | 27 | 126.8 | 63.5 | 40.2 | 38.4 | 1.65× |
| `Peano` | 5 | 74.3 | 10.4 | 2.1 | ~0 | — |

**Modules:** 5, 79 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 2.01× geomean (median 3.16×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
