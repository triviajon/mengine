# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 62.2 ms (±11.6), MEngine 4.1 ms (±0.4). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 11 | 78.3 | 16.2 | 3.4 | ~0 | — |
| `Logic` | 13 | 82.1 | 19.9 | 5.0 | 1.0 | 20.18× |
| `Nat` | 5 | 69.6 | ~0 | 3.0 | ~0 | — |
| `Peano` | 4 | 69.2 | ~0 | 2.1 | ~0 | — |

**Modules:** 4, 33 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 3 of 4.
**Proof-only speedup (Rocq/MEngine), over the 1 module(s) above the noise floor:** 20.18× geomean (median 20.18×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
