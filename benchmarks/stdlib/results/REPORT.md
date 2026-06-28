# Rocq stdlib benchmark — MEngine vs Rocq

Whole-file wall-clock is dominated by fixed per-invocation cost — a `coqc` process plus its auto-loaded `Prelude`, and an `mengine` process plus `prelude/tactics.me` + the compat prelude — none of which is the unit's proof. To compare *proof* cost fairly we time each engine's preamble alone (an empty `.v`; the compat prelude with no unit) and subtract it. **Proof** columns are this startup-subtracted residual (best of N trials, clamped at 0).

**Startup floor:** Rocq 64.9 ms (±5.6), MEngine 1.7 ms (±1.6). A proof residual at or below its engine's jitter (±) is reported as `~0` — indistinguishable from startup.

| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | MEngine total (ms) | MEngine proof (ms) | Proof speedup |
|--------|--------|-----------------|-----------------|--------------------|--------------------|---------------|
| `Bool` | 18 | 85.8 | 20.9 | 5.8 | 4.1 | 5.15× |
| `Lists` | 4 | 74.9 | 10.0 | 28.9 | 27.2 | 0.37× |
| `Logic` | 13 | 78.7 | 13.8 | 5.4 | 3.7 | 3.77× |
| `Nat` | 13 | 97.4 | 32.5 | 16.2 | 14.5 | 2.25× |
| `Peano` | 4 | 67.0 | ~0 | 1.9 | ~0 | — |

**Modules:** 5, 52 lemmas (all Tier A) — one benchmark file per stdlib module, matching the library's own file structure.
**Below startup-noise floor (proof time ~0 on either engine):** 1 of 5.
**Proof-only speedup (Rocq/MEngine), over the 4 module(s) above the noise floor:** 2.00× geomean (median 3.77×).

> Proof columns are startup-subtracted; modules above the noise floor give a real proof-speed comparison, while the whole-file columns still include each engine's fixed per-invocation cost (which dominates the raw ratio).
