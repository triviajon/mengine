# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 62.1 ms (±10.1), MEngine 4.1 ms (±0.1). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 18 | 84.0 | 21.8 | 4.8 | 0.7 | 30.33× |
| `Logic` | 13 | 79.4 | 17.2 | 4.6 | 0.5 | 32.68× |
| `Nat` | 13 | 90.9 | 28.7 | 13.8 | 9.7 | 2.97× |
| `Peano` | 4 | 70.4 | ~0 | 2.3 | ~0 | — |

**Modules:** 4, 48 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 4.
**Proof-only speedup (Rocq/MEngine), over the 3 module(s) above the noise floor:** 14.33× geomean (median 30.33×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
