#!/usr/bin/env python3
"""Reporting for the Rocq-stdlib benchmark: markdown table + scatter plot.

Reads results/stdlib.json (written by stdlib_bench.py run) and emits:
  - stdlib/results/REPORT.md  : per-unit table + geometric-mean summary
  - stdlib/plots/stdlib_scatter.png : Rocq time (x) vs MEngine time (y), log-log
"""

import json
import math
import os


MENGINE_BASELINE_KEY = "mengine__startup_baseline"
ROCQ_BASELINE_KEY = "coq__startup_baseline"


def _baseline(results, key):
    """(floor_seconds, noise_seconds) for a stored startup baseline, or (None, 0)."""
    b = results.get(key)
    if not b or not b.get("success"):
        return None, 0.0
    floor = b["time_taken"]
    times = [t["time_taken"] for t in b.get("trials", []) if t.get("success")]
    if len(times) >= 2:
        mean = sum(times) / len(times)
        noise = (sum((t - mean) ** 2 for t in times) / (len(times) - 1)) ** 0.5
    else:
        noise = 0.0
    return floor, noise


def _proof_time(total, floor):
    """Marginal statement+proof cost: whole-file minus startup floor, clamped."""
    if total is None:
        return None
    if floor is None:
        return total
    return max(0.0, total - floor)


def _succ_times(entry):
    """Successful per-trial wall-clock times (s) for a result, newest schema first.

    Falls back to the single stored `time_taken` for older results that predate
    the per-trial array, so the scatter still draws (with zero-width bars)."""
    if not entry or not entry.get("success"):
        return []
    times = [t["time_taken"] for t in entry.get("trials", []) if t.get("success")]
    return times or ([entry["time_taken"]] if entry.get("time_taken") else [])


def _pct(sorted_xs, p):
    """p-th percentile (0..100) of an already-sorted list, linear interpolation."""
    if not sorted_xs:
        return None
    if len(sorted_xs) == 1:
        return sorted_xs[0]
    k = (len(sorted_xs) - 1) * (p / 100.0)
    lo = int(math.floor(k))
    hi = int(math.ceil(k))
    if lo == hi:
        return sorted_xs[lo]
    return sorted_xs[lo] + (sorted_xs[hi] - sorted_xs[lo]) * (k - lo)


def _residual_dist(totals, floors):
    """Empirical distribution of the proof residual (total - startup floor).

    The residual we plot is `whole-file time - startup floor`, and *both* inputs
    are measured with run-to-run jitter.  Treating the two as independent, every
    `t - f` pair (t a whole-file trial, f a startup-baseline trial) is one draw of
    the residual under independent resampling; the spread of that set is the
    combined jitter of the startup time and the total time.  Returns the sorted
    differences (may include negatives when floor noise swamps a tiny proof)."""
    if not totals or not floors:
        return []
    return sorted(t - f for t in totals for f in floors)


def _lemma_counts(cfg):
    """Map module name -> number of statements, read from the corpus manifest."""
    path = os.path.join(cfg.get("corpus_dir", ""), "manifest.json")
    if not os.path.exists(path):
        return {}
    with open(path) as f:
        manifest = json.load(f)
    return {u["name"]: len(u.get("statements", [])) for u in manifest.get("units", [])}


def _load(cfg):
    with open(cfg["results"]) as f:
        results = json.load(f)
    m_floor, m_noise = _baseline(results, MENGINE_BASELINE_KEY)
    r_floor, r_noise = _baseline(results, ROCQ_BASELINE_KEY)
    # MEngine records only one global startup baseline; Rocq records one per module
    # (its own Require/Import preamble).  Keep the raw trial arrays so the scatter
    # can show how noisy each floor is, not just its best value.
    m_floor_trials = _succ_times(results.get(MENGINE_BASELINE_KEY))
    counts = _lemma_counts(cfg)
    # Per-unit keys are `mengine_<u>` / `coq_<u>` (single underscore); every
    # baseline key uses a double underscore, so exclude those.
    unit_keys = {k.split("_", 1)[1] for k in results if "_" in k and "__" not in k}
    rows = []
    for u in sorted(unit_keys):
        m = results.get(f"mengine_{u}")
        r = results.get(f"coq_{u}")
        if not m or not r:
            continue
        mt = m["time_taken"] if m["success"] else None
        rt = r["time_taken"] if r["success"] else None
        # Rocq's startup floor is this module's own Require/Import preamble (so a
        # module that loads a library has that load subtracted, not charged to the
        # proof); fall back to the global empty-.v floor if not recorded.
        ru_floor, ru_noise = _baseline(results, f"coq__baseline__{u}")
        ru_floor_trials = _succ_times(results.get(f"coq__baseline__{u}"))
        if ru_floor is None:
            ru_floor, ru_noise = r_floor, r_noise
            ru_floor_trials = _succ_times(results.get(ROCQ_BASELINE_KEY))
        rows.append({
            # Each unit is a stdlib module file (Bool/Logic/Nat/Peano); category == module.
            "unit": u, "category": u, "nlemmas": counts.get(u),
            "mengine": mt, "rocq": rt,
            "mengine_proof": _proof_time(mt, m_floor),
            "rocq_proof": _proof_time(rt, ru_floor),
            "rocq_floor": ru_floor, "rocq_noise": ru_noise,
            # Per-trial arrays (seconds) for the error-bar scatter.
            "mengine_total_trials": _succ_times(m),
            "mengine_floor_trials": m_floor_trials,
            "rocq_total_trials": _succ_times(r),
            "rocq_floor_trials": ru_floor_trials,
        })
    meta = {
        "mengine_floor": m_floor, "mengine_noise": m_noise,
        "rocq_floor": r_floor, "rocq_noise": r_noise,
        "nlemmas": sum(counts.values()) if counts else None,
    }
    return rows, meta


def _fmt_ms(t):
    return f"{t*1000:.1f}" if t is not None else "FAIL"


def _geo_med(speedups):
    """Geometric mean and median of a non-empty list of speedups."""
    geo = math.exp(sum(math.log(s) for s in speedups) / len(speedups))
    return geo, sorted(speedups)[len(speedups) // 2]


def generate(cfg):
    rows, meta = _load(cfg)
    if not rows:
        print("No results yet — run `stdlib_bench.py run` first.")
        return

    m_floor, m_noise = meta["mengine_floor"], meta["mengine_noise"]
    r_floor, r_noise = meta["rocq_floor"], meta["rocq_noise"]
    # The report renders one table: startup-subtracted *proof* time per module.
    # That needs both engines' startup floors, which `stdlib_bench.py run` always
    # records (see cmd_run) — so a results file without them is stale, not a
    # supported mode.  Bail with an actionable message rather than degrade to a
    # whole-file table that buries the proof cost under process startup.
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
        "no unit) and subtract it. **Proof** columns are this "
        "startup-subtracted residual (best of N trials, clamped at 0).\n",
        f"**Startup floor:** Rocq {_fmt_ms(r_floor)} ms "
        f"(±{r_noise*1000:.1f}), MEngine {_fmt_ms(m_floor)} ms "
        f"(±{m_noise*1000:.1f}). A proof residual at or below its engine's "
        "jitter (±) is reported as `~0` — indistinguishable from startup.\n",
        "| Module | Lemmas | Rocq total (ms) | Rocq proof (ms) | "
        "MEngine total (ms) | MEngine proof (ms) | Proof speedup |",
        "|--------|--------|-----------------|-----------------|"
        "--------------------|--------------------|---------------|",
    ]

    speedups = []
    below_noise = 0
    for row in sorted(rows, key=lambda r: r["unit"]):
        nl = row["nlemmas"] if row["nlemmas"] is not None else "?"
        mp, rp = row["mengine_proof"], row["rocq_proof"]
        m_below = mp is not None and mp <= m_noise
        r_below = rp is not None and rp <= row.get("rocq_noise", r_noise)
        mp_s = "~0" if m_below else _fmt_ms(mp)
        rp_s = "~0" if r_below else _fmt_ms(rp)
        if m_below or r_below:
            below_noise += 1
            sp_s = "—"
        elif mp and rp and mp > 0:
            sp = rp / mp
            speedups.append(sp)
            sp_s = f"{sp:.2f}×"
        else:
            sp_s = "-"
        lines.append(f"| `{row['unit']}` | {nl} | "
                     f"{_fmt_ms(row['rocq'])} | {rp_s} | "
                     f"{_fmt_ms(row['mengine'])} | {mp_s} | {sp_s} |")

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


def _scatter(cfg, rows, meta=None):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except Exception as e:  # noqa: BLE001
        print(f"(skipping scatter plot: matplotlib unavailable: {e})")
        return

    meta = meta or {}
    colors = {"Bool": "tab:blue", "Lists": "tab:purple", "Logic": "tab:red",
              "Nat": "tab:green", "Peano": "tab:orange"}
    m_floor = meta.get("mengine_floor")
    m_noise = meta.get("mengine_noise") or 0.0

    # Plot *startup-subtracted proof times*, not whole-file times.  This is the
    # one honest axis choice when modules have different startup floors: `Lists`
    # `Require`s `Coq.Lists.List`, so its Rocq floor (~128 ms) is double every
    # other module's (~62 ms).  A whole-file scatter charges that library load to
    # Lists' x-coordinate, and a single parity line — anchored at the *global*
    # Rocq floor — credits Rocq only ~62 ms of it, so Lists drops below parity
    # and reads as an MEngine win when MEngine's proof is in fact ~2× slower.
    # Each point here instead carries its *own* module's floor (subtracted in
    # `_load` via `coq__baseline__<unit>`), so the axes are directly comparable
    # and the parity line is the honest y = x: above it MEngine's proof is
    # slower, below it faster.  (Whole-file numbers remain in REPORT.md's table.)
    # Whiskers span the 10th–90th percentile of the resampled residual; the dot is
    # the median.  Anything inside this band is consistent with the measured noise.
    PLO, PHI = 10, 90

    def _axis_stat(totals, floors, fallback):
        """(median, p10, p90) of the residual in ms; zero-width if no trial array."""
        d = _residual_dist(totals, floors)
        if not d:
            v = (fallback or 0.0) * 1000.0
            return v, v, v
        return _pct(d, 50) * 1000.0, _pct(d, PLO) * 1000.0, _pct(d, PHI) * 1000.0

    # Each module becomes a cross: median residual on each axis, with x/y whiskers
    # carrying the *combined* run-to-run jitter of its whole-file time and its
    # startup floor (see `_residual_dist`).  A module is plotted when its median
    # residual clears zero on both axes; one whose median is buried in startup
    # noise (median total <= median floor) is dropped, same as before.
    pts = []
    dropped = []
    for r in rows:
        cx, xlo, xhi = _axis_stat(r["rocq_total_trials"], r["rocq_floor_trials"],
                                  r.get("rocq_proof"))
        cy, ylo, yhi = _axis_stat(r["mengine_total_trials"], r["mengine_floor_trials"],
                                  r.get("mengine_proof"))
        if cx <= 0 or cy <= 0:
            dropped.append(r["unit"])
            continue
        pts.append({"r": r, "cx": cx, "xlo": xlo, "xhi": xhi,
                    "cy": cy, "ylo": ylo, "yhi": yhi})
    if not pts:
        print("(skipping scatter plot: no module has a measurable proof residual; "
              f"all at/below startup floor: {', '.join(dropped) or 'none'})")
        return
    if dropped:
        print(f"(scatter: {', '.join(dropped)} omitted — median proof residual <= 0, "
              "i.e. indistinguishable from startup)")

    fig, ax = plt.subplots(figsize=(6.4, 6.4))

    # Axis bounds from the medians and the upper whiskers only — never the lower
    # whiskers.  A residual whose 10th percentile dips toward (or below) zero would,
    # on a log scale, drag `lo` to ~0 and crush every point into the top corner; we
    # instead let such a whisker run off the bottom edge (clamped below) as an
    # honest "could be startup noise" signal, and keep the frame where the data is.
    centers = [p["cx"] for p in pts] + [p["cy"] for p in pts]
    uppers = [p["xhi"] for p in pts] + [p["yhi"] for p in pts]
    lo = min(centers) * 0.5
    hi = max(centers + uppers) * 1.7

    # Honest parity: equal proof time on both engines is y = x (startup already
    # removed per module).  Shade the MEngine-faster half so the read is obvious.
    ax.plot([lo, hi], [lo, hi], "k--", lw=1, label="parity (equal proof time)")
    ax.fill_between([lo, hi], [lo, lo], [lo, hi], color="tab:green", alpha=0.05)
    ax.text(hi * 0.85, lo * 2.4, "MEngine faster", fontsize=8, ha="right",
            va="bottom", color="tab:green", style="italic")
    ax.text(lo * 6, hi * 0.18, "MEngine slower", fontsize=8, ha="left",
            va="center", color="tab:red", style="italic", rotation=45)

    # Reliability floor: a residual at/below the baseline's own run-to-run jitter
    # is not distinguishable from startup.  Shade that band so borderline points
    # (e.g. Peano) are read with appropriate caution rather than as hard numbers.
    if m_floor is not None and m_noise > 0:
        ax.axhspan(lo, m_noise * 1000, color="tab:gray", alpha=0.12, zorder=0)
        ax.text(hi * 0.96, m_noise * 1000, "MEngine noise floor", fontsize=6,
                ha="right", va="bottom", color="dimgray")

    # One error-bar cross per module.  Whisker ends are clamped to the visible frame
    # so a residual whose 10th-percentile dips into (or below) zero is drawn running
    # off the bottom/left edge — an honest "this could be startup noise" signal.
    seen = set()
    for p in pts:
        r = p["r"]
        col = colors.get(r["category"], "gray")
        xerr = [[p["cx"] - max(p["xlo"], lo)], [min(p["xhi"], hi) - p["cx"]]]
        yerr = [[p["cy"] - max(p["ylo"], lo)], [min(p["yhi"], hi) - p["cy"]]]
        label = r["category"] if r["category"] not in seen else None
        seen.add(r["category"])
        ax.errorbar(p["cx"], p["cy"], xerr=xerr, yerr=yerr, fmt="o", color=col,
                    ecolor=col, elinewidth=1.1, capsize=3, capthick=1.1,
                    markersize=6, markeredgecolor="k", markeredgewidth=0.4,
                    alpha=0.9, zorder=3, label=label)
        ax.annotate(r["unit"], (p["cx"], p["cy"]), textcoords="offset points",
                    xytext=(6, 4), fontsize=7)

    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xlim(lo, hi); ax.set_ylim(lo, hi)
    ax.set_aspect("equal")
    ax.set_xlabel("Rocq proof time (ms, log) — startup-subtracted")
    ax.set_ylabel("MEngine proof time (ms, log) — startup-subtracted")
    ax.set_title("Rocq stdlib modules: proof cost (per-module startup removed)\n"
                 f"dot = median residual; whiskers = {PLO}–{PHI}% of "
                 "(total ⊖ startup) jitter")
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
