#!/usr/bin/env python3
"""Reporting for the Rocq-stdlib benchmark: markdown table + scatter plot.

Reads results/stdlib.json (written by stdlib_bench.py run) and emits:
  - stdlib/results/REPORT.md         : per-module table + geometric-mean summary
  - stdlib/plots/stdlib_scatter.png  : Rocq proof time (x) vs MEngine (y), log-log

The whole report rests on two textbook numbers per measurement, so it is easy to
check by hand:

  * value  = the *minimum* over its trials (best of N — the run least perturbed
             by the OS, the standard wall-clock estimator);
  * noise  = the *sample standard deviation* of the startup-floor trials (the
             jitter below which a difference is not meaningful).

From those: proof = max(0, min(whole-file) - min(startup floor)); a proof at or
below its engine's noise is shown as `~0`; speedup = Rocq proof / MEngine proof.
There is no resampling, percentile, or error-bar math — the plot draws exactly
the table's proof numbers.
"""

import json
import math
import os

# Single source of truth for the results-file key scheme, so the writer
# (stdlib_bench) and this reader can never drift apart.  stdlib_bench imports
# `report` only lazily (inside cmd_report), so this top-level import is not
# circular.
from stdlib_bench import (MENGINE_BASELINE_KEY, ROCQ_BASELINE_KEY,
                          rocq_module_baseline_key)


# ─────────────────────────────── basic stats ─────────────────────────────────

def _times(entry):
    """Successful per-trial wall-clock times (seconds); [] if missing/failed."""
    if not entry or not entry.get("success"):
        return []
    return [t["time_taken"] for t in entry.get("trials", []) if t.get("success")]


def _stddev(xs):
    """Sample standard deviation of a list (0.0 for fewer than two points)."""
    if len(xs) < 2:
        return 0.0
    mean = sum(xs) / len(xs)
    return (sum((x - mean) ** 2 for x in xs) / (len(xs) - 1)) ** 0.5


def _proof(total_times, floor_times):
    """Best-of-N proof-only cost: min(whole-file) - min(startup floor), clamped.

    None when either measurement is missing — with no floor to subtract the
    residual is undefined, and reporting the raw total (startup included) as the
    proof cost would defeat the whole subtraction."""
    if not total_times or not floor_times:
        return None
    return max(0.0, min(total_times) - min(floor_times))


def _median(xs):
    """Median of a non-empty list (mean of the two central values when even)."""
    s = sorted(xs)
    n = len(s)
    mid = n // 2
    return s[mid] if n % 2 else (s[mid - 1] + s[mid]) / 2.0


def _geo_med(speedups):
    """Geometric mean and median of a non-empty list of speedups."""
    geo = math.exp(sum(math.log(s) for s in speedups) / len(speedups))
    return geo, _median(speedups)


def _fmt_ms(t):
    return f"{t*1000:.1f}" if t is not None else "FAIL"


# ─────────────────────────────── data loading ────────────────────────────────

def _lemma_counts(cfg):
    """Map module name -> number of statements, read from the corpus manifest."""
    path = os.path.join(cfg.get("corpus_dir", ""), "manifest.json")
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        manifest = json.load(f)
    return {u["name"]: len(u.get("statements", [])) for u in manifest.get("units", [])}


def _load(cfg):
    """Return (rows, meta): one row per module, plus the global startup floors.

    Each row's `*_proof` is the module's startup-subtracted proof time and each
    `*_noise` is the jitter of the floor that was subtracted.  MEngine has one
    global floor; Rocq has one *per module* (its own Require/Import preamble —
    e.g. Lists `Require`s List, so its floor is ~2× the others), falling back to
    the global empty-.v floor when a module's own baseline wasn't recorded.

    Every value here is recomputed from the raw per-trial times (via `_times`),
    never read from the runner's stored `time_taken` — so the report is a
    self-contained derivation an auditor can recheck against the trial arrays."""
    with open(cfg["results"]) as f:
        results = json.load(f)

    m_floor_times = _times(results.get(MENGINE_BASELINE_KEY))
    r_floor_times = _times(results.get(ROCQ_BASELINE_KEY))
    counts = _lemma_counts(cfg)

    # Per-module keys are `mengine_<u>` / `coq_<u>`; baseline keys double the
    # underscore after the engine prefix (`mengine__…`, `coq__…`).  Strip a
    # single-underscore prefix to recover the module name (keeps names that
    # contain '_' intact, unlike splitting on the first '_').
    unit_keys = set()
    for prefix in ("mengine_", "coq_"):
        unit_keys |= {k[len(prefix):] for k in results
                      if k.startswith(prefix) and not k.startswith(prefix + "_")}

    rows = []
    for u in sorted(unit_keys):
        m_total = _times(results.get(f"mengine_{u}"))
        r_total = _times(results.get(f"coq_{u}"))
        if not m_total or not r_total:
            continue
        r_mod_floor = _times(results.get(rocq_module_baseline_key(u))) or r_floor_times
        rows.append({
            "unit": u,
            "nlemmas": counts.get(u),
            "mengine_total": min(m_total),
            "rocq_total": min(r_total),
            "mengine_proof": _proof(m_total, m_floor_times),
            "rocq_proof": _proof(r_total, r_mod_floor),
            "mengine_noise": _stddev(m_floor_times),
            "rocq_noise": _stddev(r_mod_floor),
        })
    meta = {
        "mengine_floor": min(m_floor_times) if m_floor_times else None,
        "rocq_floor": min(r_floor_times) if r_floor_times else None,
        "mengine_noise": _stddev(m_floor_times),
        "rocq_noise": _stddev(r_floor_times),
        "nlemmas": sum(counts.values()) if counts else None,
    }
    return rows, meta


# ───────────────────────────────── report ────────────────────────────────────

def generate(cfg):
    rows, meta = _load(cfg)
    if not rows:
        print("No results yet — run `stdlib_bench.py run` first.")
        return

    m_floor, m_noise = meta["mengine_floor"], meta["mengine_noise"]
    r_floor, r_noise = meta["rocq_floor"], meta["rocq_noise"]
    # The proof columns subtract each engine's startup floor, which
    # `stdlib_bench.py run` always records (see cmd_run) — so a results file
    # without them is stale, not a supported mode.  Bail with an actionable
    # message rather than degrade to a whole-file table that buries proof cost
    # under process startup.
    if m_floor is None or r_floor is None:
        print("No startup baseline in results — re-run `stdlib_bench.py run` "
              "(it records each engine's startup floor, which the proof columns "
              "subtract).")
        return

    lines = [
        "# Rocq stdlib benchmark — MEngine vs Rocq\n",
        "Whole-file wall-clock is dominated by fixed per-invocation cost — a "
        "`coqc` process plus its auto-loaded `Prelude`, and an `mengine` "
        "process plus `prelude/tactics.me` + the compat prelude — none of "
        "which is the unit's proof. To compare *proof* cost fairly we time "
        "each engine's preamble alone (an empty `.v`; the compat prelude with "
        "no unit) and subtract it. Every number below is the **best (minimum) "
        "of N trials**; **proof** columns are that whole-file minimum minus the "
        "startup-floor minimum, clamped at 0.\n",
        f"**Startup floor:** Rocq {_fmt_ms(r_floor)} ms "
        f"(±{r_noise*1000:.1f}), MEngine {_fmt_ms(m_floor)} ms "
        f"(±{m_noise*1000:.1f}). The ± is the standard deviation of the floor's "
        "trials; a proof at or below its engine's ± is reported as `~0` — "
        "indistinguishable from startup.\n",
        "| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | "
        "MEngine total (ms) | MEngine proof (ms) | Proof speedup |",
        "|--------|--------|-----------------|-----------------|"
        "--------------------|--------------------|---------------|",
    ]

    speedups = []
    below_noise = 0
    for row in rows:
        nl = row["nlemmas"] if row["nlemmas"] is not None else "?"
        mp, rp = row["mengine_proof"], row["rocq_proof"]
        m_below = mp is not None and mp <= row["mengine_noise"]
        r_below = rp is not None and rp <= row["rocq_noise"]
        mp_s = "~0" if m_below else _fmt_ms(mp)
        rp_s = "~0" if r_below else _fmt_ms(rp)
        if m_below or r_below:
            below_noise += 1
            sp_s = "—"
        elif mp and rp:
            sp = rp / mp
            speedups.append(sp)
            sp_s = f"{sp:.2f}×"
        else:
            sp_s = "-"
        lines.append(f"| `{row['unit']}` | {nl} | "
                     f"{_fmt_ms(row['rocq_total'])} | {rp_s} | "
                     f"{_fmt_ms(row['mengine_total'])} | {mp_s} | {sp_s} |")

    nlem = meta.get("nlemmas")
    lem_s = f", {nlem} lemmas" if nlem else ""
    lines.append("")
    lines.append(f"**Modules:** {len(rows)}{lem_s} (all Tier A) — one benchmark file "
                 "per stdlib module, matching the library's own file structure.")
    lines.append(f"**Below startup-noise floor (proof time ~0 on either engine):** "
                 f"{below_noise} of {len(rows)}.")
    if speedups:
        geo, med = _geo_med(speedups)
        lines.append(f"**Proof-only speedup (Rocq/MEngine), over the "
                     f"{len(speedups)} module(s) above the noise floor:** "
                     f"{geo:.2f}× geomean (median {med:.2f}×).")
    lines.append("")
    if below_noise == len(rows):
        lines.append("> Even grouped per module, these Tier-A proofs are cheap "
                     "enough that their startup-subtracted cost stays at or below "
                     "measurement jitter on both engines — so the whole-file ratio "
                     "is still mostly a *process-startup* ratio. Heavier modules "
                     "(computational induction, list reasoning) are needed to "
                     "measure proof speed clearly above the noise floor.")
    else:
        lines.append("> Proof columns are startup-subtracted; modules above the "
                     "noise floor give a real proof-speed comparison, while the "
                     "whole-file columns still include each engine's fixed "
                     "per-invocation cost (which dominates the raw ratio).")

    out_md = os.path.join(os.path.dirname(cfg["results"]), "REPORT.md")
    with open(out_md, "w") as f:
        f.write("\n".join(lines) + "\n")
    print(f"Wrote {os.path.relpath(out_md)}")

    _scatter(cfg, rows, meta)


# ─────────────────────────────── scatter plot ────────────────────────────────

def _scatter(cfg, rows, meta):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:  # noqa: BLE001
        print(f"(skipping scatter plot: matplotlib unavailable: {e})")
        return

    colors = {"Bool": "tab:blue", "Lists": "tab:purple", "Logic": "tab:red",
              "Nat": "tab:green", "Peano": "tab:orange"}
    m_noise, r_noise = meta["mengine_noise"], meta["rocq_noise"]

    # Each dot is exactly the table's (Rocq proof, MEngine proof) for a module —
    # both startup-subtracted with that module's *own* floor.  Subtracting the
    # module's own floor is the one honest axis choice when floors differ: Lists
    # `Require`s List, so its Rocq floor is ~2× the others, and a whole-file
    # scatter would charge that library load to Lists' x-coordinate and make
    # MEngine look ~2× better on Lists than it is.  A module whose proof is 0 on
    # either axis can't sit on a log scale, so it is dropped (and named).
    pts, dropped = [], []
    for r in rows:
        (pts if r["mengine_proof"] and r["rocq_proof"] else dropped).append(r)
    if not pts:
        names = ", ".join(r["unit"] for r in dropped) or "none"
        print(f"(skipping scatter plot: no module has a measurable proof "
              f"residual; all at/below startup: {names})")
        return
    if dropped:
        names = ", ".join(r["unit"] for r in dropped)
        print(f"(scatter: {names} omitted — proof residual 0 on an axis, "
              "i.e. indistinguishable from startup)")

    xs = [r["rocq_proof"] * 1000 for r in pts]
    ys = [r["mengine_proof"] * 1000 for r in pts]
    lo = min(xs + ys) * 0.5
    hi = max(xs + ys) * 1.7

    fig, ax = plt.subplots(figsize=(6.4, 6.4))

    # Parity: equal proof time on both engines is y = x (startup already removed
    # per module).  Shade the MEngine-faster half so the read is obvious.
    ax.plot([lo, hi], [lo, hi], "k--", lw=1, label="parity (equal proof time)")
    ax.fill_between([lo, hi], [lo, lo], [lo, hi], color="tab:green", alpha=0.05)
    ax.text(hi * 0.85, lo * 2.4, "MEngine faster", fontsize=8, ha="right",
            va="bottom", color="tab:green", style="italic")
    ax.text(lo * 6, hi * 0.18, "MEngine slower", fontsize=8, ha="left",
            va="center", color="tab:red", style="italic", rotation=45)

    # Noise floors: a proof at/below the startup measurement's own jitter (its
    # trials' stddev) is not distinguishable from startup.  Shade each engine's
    # band so borderline points (e.g. Peano) are read with caution.
    if m_noise > 0:
        ax.axhspan(lo, m_noise * 1000, color="tab:gray", alpha=0.12, zorder=0)
        ax.text(hi * 0.96, m_noise * 1000, "MEngine noise floor", fontsize=6,
                ha="right", va="bottom", color="dimgray")
    if r_noise > 0:
        ax.axvspan(lo, r_noise * 1000, color="tab:gray", alpha=0.10, zorder=0)

    for r in pts:
        x, y = r["rocq_proof"] * 1000, r["mengine_proof"] * 1000
        ax.plot(x, y, "o", color=colors.get(r["unit"], "gray"), markersize=7,
                markeredgecolor="k", markeredgewidth=0.5, zorder=3,
                label=r["unit"])
        ax.annotate(r["unit"], (x, y), textcoords="offset points",
                    xytext=(6, 4), fontsize=8)

    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_xlim(lo, hi)
    ax.set_ylim(lo, hi)
    ax.set_aspect("equal")
    ax.set_xlabel("Rocq proof time (ms, log) — startup-subtracted")
    ax.set_ylabel("MEngine proof time (ms, log) — startup-subtracted")
    ax.set_title("Rocq stdlib modules: proof cost (per-module startup removed)\n"
                 "point = best-of-N whole-file time − startup floor")
    ax.legend(fontsize=8, loc="upper left")
    ax.grid(True, which="both", ls=":", alpha=0.4)
    os.makedirs(cfg["plots_dir"], exist_ok=True)
    out = os.path.join(cfg["plots_dir"], "stdlib_scatter.png")
    fig.tight_layout()
    fig.savefig(out, dpi=120)
    print(f"Wrote {os.path.relpath(out)}")


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from stdlib_bench import load_config
    generate(load_config())
