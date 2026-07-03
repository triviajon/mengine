#!/usr/bin/env python3
"""Corpus runner for the Rocq-stdlib benchmark (MEngine vs Rocq).

A sibling to ``benchmarks/bench.py``: instead of sweeping a parameter range it
iterates a fixed manifest of curated, auto-translated stdlib units.  It reuses
the timing discipline of ``framework/runner.run_single`` (process-group kill on
timeout, N trials keeping the minimum) without subclassing ``Benchmark``.

Subcommands:
    test                 faithfulness gate: coqc compiles rocq.v, MEngine runs
                         mengine.me clean, and the statement digests correspond
    fidelity             verify each curated statement matches its real stdlib
                         counterpart (corpus/stdlib_map.json), via Rocq's kernel
    clean                remove every generated file (mengine.me, manifest,
                         results, plot); keep sources
    regen                rebuild every generated file from source, in dependency
                         order (mengine.me -> manifest -> run -> report)

`regen` and `clean` are the primary entry points; `test` and `fidelity` are the
correctness gates, run by hand.  The run/report/manifest steps are internal to
`regen` (no standalone subcommand).
"""

import argparse
import hashlib
import json
import os
import re
import signal
import statistics
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
BENCH_ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local module)


# ─────────────────────────────── config ──────────────────────────────────────

def load_config():
    with open(os.path.join(BENCH_ROOT, "config.json")) as f:
        cfg = json.load(f)
    s = cfg.get("stdlib", {})
    def exp(p):
        return os.path.expanduser(p) if p else p
    return {
        "mengine_path": exp(cfg["mengine_path"]),
        "mengine_root": exp(cfg.get("mengine_root", "")),
        "coq_path": exp(cfg.get("coq_path", "coqc")),
        "corpus_dir": os.path.join(BENCH_ROOT, s.get("corpus_dir", "stdlib/corpus")),
        "compat": os.path.join(BENCH_ROOT, s.get("compat", "stdlib/compat/stdlib_compat.me")),
        "results": os.path.join(BENCH_ROOT, s.get("results", "stdlib/results/stdlib.json")),
        "plots_dir": os.path.join(BENCH_ROOT, s.get("plots_dir", "stdlib/plots")),
        "timeout": s.get("timeout", 20),
        "trials": s.get("trials", 10),
        "coq_timeout_multiplier": cfg.get("coq_timeout_multiplier", 1.5),
    }


# ─────────────────────────── corpus discovery ────────────────────────────────

# Each unit is a whole stdlib *module* file — one per corpus/<Module>/ directory
# (Bool, Lists, Logic, Nat, Peano) — mirroring how the standard library is
# organized, not a one-lemma fragment.


def discover_units(cfg):
    return translate.corpus_modules(cfg["corpus_dir"])


def unit_paths(cfg, name):
    d = os.path.join(cfg["corpus_dir"], name)
    return os.path.join(d, "rocq.v"), os.path.join(d, "mengine.me"), d


def unit_statement_digests(mepath):
    """(name, digest) pairs for each Theorem in a unit's mengine.me."""
    with open(mepath) as f:
        return translate.statement_digests(f.read())


# ───────────────────────────── timing core ───────────────────────────────────
# Mirrors framework.runner.run_single: N trials, keep the min successful time,
# kill the whole process group on timeout.

def time_command(cmd, cwd, timeout, trials):
    best = None
    success = False
    all_trials = []
    last_err = ""
    for _ in range(trials):
        start = time.perf_counter()
        try:
            proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    text=True, cwd=cwd, start_new_session=True)
        except OSError as e:
            # Missing/unexecutable binary: record a failed trial and stop, like the
            # sibling scripts (translate/fidelity) — don't crash the whole run.
            last_err = f"could not run {cmd[0]!r}: {e}"
            all_trials.append({"time_taken": None, "success": False})
            break
        try:
            out, err = proc.communicate(timeout=timeout)
            elapsed = time.perf_counter() - start
            ok = proc.returncode == 0
            all_trials.append({"time_taken": elapsed, "success": ok})
            if ok:
                success = True
                if best is None or elapsed < best:
                    best = elapsed
            else:
                last_err = (err or out)[:300]
                break  # don't repeat a failing point
        except subprocess.TimeoutExpired:
            elapsed = time.perf_counter() - start
            # Mirror what subprocess.run(timeout=…) does on expiry — kill, then
            # communicate() to reap — but across the whole process group, since
            # start_new_session=True made the child a group leader.  SIGKILL can't
            # be caught, so a single killpg + communicate() reliably tears down the
            # child and any grandchildren with no lingering pipe or zombie.
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass  # already exited between timeout and kill
            proc.communicate()
            all_trials.append({"time_taken": elapsed, "success": False, "timeout": True})
            last_err = f"timeout after {timeout}s"
            break
    return {"time_taken": best,
            "success": success, "trials": all_trials, "error": last_err}


# ─────────────────────────── per-engine commands ─────────────────────────────

def mengine_combined_file(cfg, mepath):
    """Write compat-prelude + unit into a temp .me; return its path."""
    fd, path = tempfile.mkstemp(suffix=".me", prefix="stdlib_unit_")
    with os.fdopen(fd, "w") as f:
        with open(cfg["compat"]) as c:
            f.write(c.read())
        f.write("\n")
        with open(mepath) as m:
            f.write(m.read())
    return path


def run_mengine(cfg, name, timeout, trials):
    _v, mepath, _d = unit_paths(cfg, name)
    combined = mengine_combined_file(cfg, mepath)
    try:
        cmd = [cfg["mengine_path"], "-q", combined]
        res = time_command(cmd, cfg["mengine_root"] or None, timeout, trials)
    finally:
        os.remove(combined)
    return res


def run_rocq(cfg, name, timeout, trials):
    vpath, _m, d = unit_paths(cfg, name)
    cmd = [cfg["coq_path"], "-q", vpath]
    to = timeout * cfg["coq_timeout_multiplier"]
    res = time_command(cmd, d, to, trials)
    translate.clean_coqc_byproducts(vpath)  # keep the corpus rocq.v, drop .vo/.aux/…
    return res


# ─────────────────────────── startup baselines ───────────────────────────────
# Whole-file wall-clock is dominated by fixed per-invocation cost: a coqc process
# plus its auto-loaded Prelude (~65 ms here), and an mengine process plus
# prelude/tactics.me + the compat prelude (~3-6 ms).  None of that is the unit's
# proof.  We time each engine's preamble *alone* — an empty .v for Rocq (same
# Prelude every unit auto-loads; the corpus has no `Require`), and the compat
# prelude with no unit appended for MEngine — and the report subtracts it to
# isolate the marginal statement+proof cost.  Measured once and reused, so we
# spend extra trials to pin the floor down.

def run_mengine_baseline(cfg, timeout, trials):
    """Time the compat prelude with no unit: MEngine's per-invocation floor."""
    cmd = [cfg["mengine_path"], "-q", cfg["compat"]]
    return time_command(cmd, cfg["mengine_root"] or None, timeout, trials)


def run_rocq_baseline(cfg, timeout, trials):
    """Time an empty .v: coqc startup + auto-loaded Prelude, no statement."""
    fd, path = tempfile.mkstemp(suffix=".v", prefix="stdlib_baseline_")
    os.close(fd)  # empty file
    try:
        cmd = [cfg["coq_path"], "-q", path]
        to = timeout * cfg["coq_timeout_multiplier"]
        res = time_command(cmd, os.path.dirname(path), to, trials)
    finally:
        translate.clean_coqc_temp(path)  # temp .v + its coqc byproducts
    return res


def module_rocq_preamble(vpath):
    """The leading directive sentences of a corpus rocq.v (everything the
    translator drops before its first statement): its Require/Import/Open/Set
    lines.  Returns a .v source string, possibly empty (a Require-free module).

    Reuses ``translate.is_dropped`` — the translator's own drop-list — as the
    single source of truth, so the *timed* preamble here and the preamble the
    translator *discards* can never diverge (e.g. a leading ``#[...]`` attribute
    or ``Declare Scope`` is recognized by both, or by neither)."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    out = []
    for s in translate.split_sentences(text):
        s = s.strip()
        if translate.is_dropped(s):
            out.append(s + ".")
        else:
            break
    return "\n".join(out)


def run_rocq_module_baseline(cfg, name, timeout, trials):
    """Time a .v holding only this module's Require/Import preamble — the module's
    own Rocq startup floor.  A `Require`-free module yields an empty file, i.e.
    the same Prelude-only floor as the global baseline; a module that loads a
    library (Lists -> `Require Import List`) pays that load here, so the report
    subtracts it instead of charging it to the proof.  Run from the unit dir so
    load paths resolve exactly as the unit's own compile does."""
    vpath, _m, d = unit_paths(cfg, name)
    pre = module_rocq_preamble(vpath)
    fd, path = tempfile.mkstemp(suffix=".v", prefix="stdlib_pre_", dir=d)
    with os.fdopen(fd, "w") as f:
        f.write(pre + "\n")
    try:
        cmd = [cfg["coq_path"], "-q", os.path.basename(path)]
        to = timeout * cfg["coq_timeout_multiplier"]
        res = time_command(cmd, d, to, trials)
    finally:
        translate.clean_coqc_temp(path)  # temp .v + its coqc byproducts
    return res


# Keys under which baselines are stored in results (alongside per-unit keys).
# Baseline keys double the underscore right after the engine prefix
# (`mengine__…`, `coq__…`) so report's per-unit key discovery — which strips a
# single-underscore engine prefix (`mengine_` / `coq_`) — skips them, keeping
# module names that themselves contain '_' intact.
MENGINE_BASELINE_KEY = "mengine__startup_baseline"
ROCQ_BASELINE_KEY = "coq__startup_baseline"


def rocq_module_baseline_key(name):
    return f"coq__baseline__{name}"


# ─────────────────────── results interpretation ──────────────────────────────
# How a stored timing result becomes a proof-cost number.  Imported by `report`
# and used by `cmd_run`'s progress line, so the console summary and REPORT.md
# apply the exact same definitions (best-of-N proof residual, stddev noise
# floor) and can never disagree.

def successful_times(entry):
    """Successful per-trial wall-clock times (seconds); [] if missing/failed."""
    if not entry or not entry.get("success"):
        return []
    return [t["time_taken"] for t in entry.get("trials", []) if t.get("success")]


def proof_time(total_times, floor_times):
    """Best-of-N proof-only cost: min(whole-file) − min(startup floor), clamped.
    None when either measurement is missing (nothing to subtract → undefined,
    never the raw total, which would count startup as proof cost)."""
    if not total_times or not floor_times:
        return None
    return max(0.0, min(total_times) - min(floor_times))


# ─────────────────────────── faithfulness gate ───────────────────────────────

def rocq_statement_names(vpath):
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    pat = r"\b(?:" + translate.THEOREM_ALT + \
          r")\s+([A-Za-z_][A-Za-z0-9_']*)"
    return [m.group(1) for m in re.finditer(pat, text)]


def test_unit(cfg, name):
    vpath, mepath, d = unit_paths(cfg, name)
    problems = []

    # 1. Rocq compiles.
    rr = run_rocq(cfg, name, cfg["timeout"], 1)
    if not rr["success"]:
        problems.append(f"rocq.v fails coqc: {rr['error'][:120]}")

    # 2. MEngine runs clean.
    mr = run_mengine(cfg, name, cfg["timeout"], 1)
    if not mr["success"]:
        problems.append(f"mengine.me fails: {mr['error'][:120]}")

    # 3. mengine.me re-derivable from rocq.v by the translator (no drift), and
    #    statement names correspond.  Statement types are elaborated through Rocq
    #    (`Set Printing All`), so the gate runs the translator's own pipeline
    #    (rocq_elaborate -> translate_unit) directly, exactly as translate.py's
    #    CLI does.
    try:
        with open(vpath) as f:
            vtext = f.read()
        elab = translate.rocq_elaborate(vtext, vpath, cfg["coq_path"],
                                        translate.statement_names(vtext))
        me_src = translate.translate_unit(vtext, translate.Report(), elab)
    except translate.Untranslatable as e:
        problems.append(f"translator flags rocq.v: {e.reason[:120]}")
    else:
        with open(mepath) as f:
            on_disk = f.read()
        if _norm(on_disk) != _norm(me_src):
            problems.append("mengine.me is stale vs translate.py output (re-run manifest)")
        me_names = set(dict(translate.statement_digests(me_src)).keys())
        rq_names = set(rocq_statement_names(vpath))
        if me_names != rq_names:
            problems.append(f"statement names differ: rocq={rq_names} mengine={me_names}")

    return problems


def _norm(s):
    return "\n".join(line.rstrip() for line in s.strip().splitlines())


# ───────────────────────────── manifest ──────────────────────────────────────

def sha256_file(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()[:16]


def compute_manifest(cfg):
    """The manifest dict for the current corpus (pure — no I/O), so callers can
    either write it (`build_manifest`) or diff the on-disk copy against it to
    detect drift (`manifest_staleness`)."""
    units = []
    for name in discover_units(cfg):
        vpath, mepath, _d = unit_paths(cfg, name)
        digests = unit_statement_digests(mepath)
        units.append({
            "name": name,
            "rocq_sha256": sha256_file(vpath),
            "statements": [{"name": n, "digest": dg} for n, dg in digests],
            "deps": "Coq.Init (auto-loaded)",
        })
    return {
        "description": "Rocq stdlib units auto-translated with zero manual "
                       "edits and proved by MEngine + the compat prelude.",
        "engine_note": "Requires the cbv applied-fix and GC-dedup kernel fixes "
                       "(see benchmarks/stdlib/README.md).",
        "units": units,
        "excluded": EXCLUDED_DOC,
    }


def build_manifest(cfg):
    manifest = compute_manifest(cfg)
    out = os.path.join(cfg["corpus_dir"], "manifest.json")
    with open(out, "w") as f:
        json.dump(manifest, f, indent=2)
    return out, len(manifest["units"])


def manifest_staleness(cfg):
    """Reason corpus/manifest.json differs from a freshly computed manifest, or
    None if current.  Guards against a corpus edit (a re-generated rocq.v /
    mengine.me) that was not followed by `stdlib_bench.py manifest`, which would
    otherwise leave stale statement digests and rocq.v hashes with no failing
    check to catch it."""
    path = os.path.join(cfg["corpus_dir"], "manifest.json")
    if not os.path.exists(path):
        return "is missing (run: stdlib_bench.py manifest)"
    with open(path) as f:
        on_disk = json.load(f)
    if on_disk != compute_manifest(cfg):
        return "is out of date vs the corpus (rocq.v / mengine.me changed?)"
    return None


# Constructs the translator deliberately does not handle; see README.
EXCLUDED_DOC = [
    {"pattern": "multi-variable case analysis / nested induction "
                "(e.g. andb_comm, orb b1 b2 = orb b2 b1, de Morgan)",
     "boundary": "nested",
     "reason": "the translator emits one `apply (<T>_ind motive)` per proof and "
               "segments a single level of cases; a second destruct inside a case "
               "(needed to decide a goal in two booleans) is not yet generated."},
    {"pattern": "induction over an inductive relation "
                "(e.g. le_trans, le_n_S via le_ind)",
     "boundary": "relational",
     "reason": "the eliminator has a dependent motive over the derivation; the "
               "translator only builds non-dependent `fun (x:T) => <body>` motives."},
]


# ─────────────────────────────── subcommands ─────────────────────────────────

def cmd_test(cfg):
    units = discover_units(cfg)
    npass = 0
    for u in units:
        problems = test_unit(cfg, u)
        if problems:
            print(f"  [FAIL] {u}")
            for p in problems:
                print(f"          {p}")
        else:
            print(f"  [ OK ] {u}")
            npass += 1
    print(f"\n{npass}/{len(units)} units pass the faithfulness gate.")
    print("(statement-vs-stdlib correspondence is a separate check: "
          "stdlib_bench.py fidelity)")
    # The manifest is a single global artifact (not per-unit), so verify it once:
    # a re-generated rocq.v / mengine.me must not leave its digests stale.
    stale = manifest_staleness(cfg)
    if stale:
        print(f"\n  [STALE] corpus/manifest.json {stale}")
        print("          re-run: stdlib_bench.py regen")
    return 0 if npass == len(units) and not stale else 1


def cmd_run(cfg):
    units = discover_units(cfg)
    results = load_results(cfg["results"])

    # Measure each engine's fixed startup/preamble floor once (extra trials to
    # pin its min and stddev down — the report subtracts the min from every
    # unit's whole-file time and uses the stddev as the noise floor).  Measured
    # once and reused, so the extra trials are cheap.  See run_*_baseline above.
    base_trials = max(cfg["trials"], 25)
    mb = run_mengine_baseline(cfg, cfg["timeout"], base_trials)
    rb = run_rocq_baseline(cfg, cfg["timeout"], base_trials)
    results[MENGINE_BASELINE_KEY] = mb
    results[ROCQ_BASELINE_KEY] = rb
    save_results(cfg["results"], results)
    mb_t, rb_t = mb["time_taken"], rb["time_taken"]
    print(f"  {'startup baseline':24s} "
          f"MEngine={mb_t*1000:8.1f}ms  Rocq={rb_t*1000:8.1f}ms\n")

    m_floor = successful_times(mb)          # MEngine's one global startup floor
    m_noise = statistics.stdev(m_floor)
    r_floor = successful_times(rb)          # global fallback for a Require-free module

    for u in units:
        # Each module's own Rocq startup floor: its Require/Import preamble timed
        # alone.  Require-free modules match the global empty-.v floor; Lists
        # (`Require Import List`) pays the library load here so the report does
        # not charge it to the proof.
        rmb = run_rocq_module_baseline(cfg, u, cfg["timeout"], base_trials)
        results[rocq_module_baseline_key(u)] = rmb

        mr = run_mengine(cfg, u, cfg["timeout"], cfg["trials"])
        rr = run_rocq(cfg, u, cfg["timeout"], cfg["trials"])
        results[f"mengine_{u}"] = mr
        results[f"coq_{u}"] = rr
        save_results(cfg["results"], results)

        # Interpret this module's timings with the *same* functions the report
        # uses (proof_time / statistics.stdev), so this progress line and REPORT.md
        # can never disagree: proof = min(whole-file) − min(startup floor); a
        # residual at/below the startup floor's own stddev prints as `~0`
        # (indistinguishable from startup) with speedup `—`, exactly as reported.
        ru_floor = successful_times(rmb) or r_floor
        mp = proof_time(successful_times(mr), m_floor)
        rp = proof_time(successful_times(rr), ru_floor)
        m_below = mp is not None and mp <= m_noise
        r_below = rp is not None and rp <= statistics.stdev(ru_floor)
        ms = f"{mr['time_taken']*1000:.1f}ms" if mr["success"] else "FAIL"
        rs = f"{rr['time_taken']*1000:.1f}ms" if rr["success"] else "FAIL"
        mps = "~0" if m_below else (f"{mp*1000:.1f}ms" if mp is not None else "FAIL")
        rps = "~0" if r_below else (f"{rp*1000:.1f}ms" if rp is not None else "FAIL")
        if m_below or r_below:
            ratio = "—"
        elif mp and rp:
            ratio = f"{rp/mp:.2f}x"
        else:
            ratio = "-"
        print(f"  {u:24s} MEngine={ms:>9s}(proof {mps:>8s})  "
              f"Rocq={rs:>9s}(proof {rps:>8s})  proof-speedup={ratio}")
    print(f"\nResults written to {os.path.relpath(cfg['results'])}")


def cmd_report(cfg):
    import report
    report.generate(cfg)


def cmd_manifest(cfg):
    out, n = build_manifest(cfg)
    print(f"Wrote {os.path.relpath(out)} ({n} units).")


def cmd_fidelity(cfg):
    import fidelity
    return fidelity.run(cfg)


# ───────────────────────── generated artifacts ───────────────────────────────
# The complete list of files the benchmark generates — the single source of
# truth for `clean` (remove them) and `regen` (rebuild them).  Everything else
# in the tree is hand-authored source: rocq.v, stdlib_map.json, the compat
# prelude, the .py scripts, and the docs.

def generated_files(cfg):
    """Absolute paths of every file the benchmark generates."""
    paths = []
    for name in discover_units(cfg):
        _v, mepath, _d = unit_paths(cfg, name)
        paths.append(mepath)                                   # corpus/<M>/mengine.me
    results_dir = os.path.dirname(cfg["results"])
    paths += [
        os.path.join(cfg["corpus_dir"], "manifest.json"),
        cfg["results"],                                        # results/stdlib.json
        os.path.join(results_dir, "REPORT.md"),
        os.path.join(cfg["plots_dir"], "stdlib_scatter.png"),
    ]
    return paths


def regen_mengine_sources(cfg):
    """Re-translate every corpus rocq.v to its mengine.me, in-process via the
    translator's own pipeline (rocq_elaborate -> translate_unit) — byte-identical
    to translate.py's CLI.  Yields each module name as its file is written;
    propagates translate.Untranslatable if a unit stops being translatable (a
    real regression, surfaced rather than hidden)."""
    for name in discover_units(cfg):
        vpath, mepath, _d = unit_paths(cfg, name)
        with open(vpath) as f:
            vtext = f.read()
        elab = translate.rocq_elaborate(vtext, vpath, cfg["coq_path"],
                                        translate.statement_names(vtext))
        me_src = translate.translate_unit(vtext, translate.Report(), elab)
        with open(mepath, "w") as f:
            f.write(me_src)
        yield name


def cmd_clean(cfg):
    removed = 0
    for p in generated_files(cfg):
        if os.path.exists(p):
            os.remove(p)
            print(f"  removed {os.path.relpath(p)}")
            removed += 1
    print(f"\n{removed} generated file(s) removed "
          "(sources — rocq.v, stdlib_map.json, compat, docs — kept).")


def cmd_regen(cfg):
    print("== 1/4  corpus mengine.me (re-translate each rocq.v) ==")
    try:
        for n in regen_mengine_sources(cfg):
            print(f"  [regen] {n}")
    except translate.Untranslatable as e:
        print(f"  [FAIL ] translator flagged a unit: {e.reason}")
        return 1
    print("\n== 2/4  corpus/manifest.json ==")
    cmd_manifest(cfg)
    print("\n== 3/4  results/stdlib.json (timing both engines) ==")
    cmd_run(cfg)
    print("\n== 4/4  results/REPORT.md + plots/stdlib_scatter.png ==")
    cmd_report(cfg)
    print("\nAll generated files rebuilt.")
    return 0


# ───────────────────────────── results io ────────────────────────────────────

def load_results(path):
    if os.path.exists(path):
        with open(path) as f:
            return json.load(f)
    return {}


def save_results(path, results):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(results, f, indent=2)


# ──────────────────────────────── main ───────────────────────────────────────

def main():
    cfg = load_config()
    ap = argparse.ArgumentParser(description="Rocq-stdlib benchmark corpus runner")
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("test", "fidelity", "clean", "regen"):
        sub.add_parser(name)
    args = ap.parse_args()

    dispatch = {
        "test": cmd_test, "fidelity": cmd_fidelity,
        "clean": cmd_clean, "regen": cmd_regen,
    }
    rc = dispatch[args.command](cfg)
    sys.exit(rc or 0)


if __name__ == "__main__":
    main()
