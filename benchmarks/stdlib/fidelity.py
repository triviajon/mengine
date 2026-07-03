#!/usr/bin/env python3
"""Statement-level fidelity check: curated corpus lemma vs real Rocq stdlib.

Corpus `rocq.v` files hand-restate stdlib lemmas.  This check asks Rocq's own
kernel to confirm each restatement matches the library lemma it claims to be,
and that the lemma actually lives in the stdlib file its module points at.

`stdlib_map.json` maps every curated lemma to a stdlib `ref` and each module to
the `module_files` its lemmas come from.  We qualify the ref with a file
(`andb_diag` -> `Stdlib.Bool.Bool.andb_diag`) so one Rocq check proves both
membership and type-equality.  Two relations:
  convertible  Check (<file>.<ref> : <curated type>).  -- types must be equal
  symmetry     prove <curated> via `symmetry; apply <file>.<ref>` -- mirror lemma

Every curated lemma must be mapped to a real stdlib ref; unmapped lemmas, stale
map entries, and refs absent from the module's files are all failures.
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local; reuse its comment/sentence/paren splitters)

# `Lemma foo (x : nat) : P x` -> name="foo", rest="(x : nat) : P x".
STMT_RE = re.compile(r"^(?:" + translate.THEOREM_ALT +
                     r")\s+([A-Za-z_][A-Za-z0-9_']*)\s*(.*)$", re.S)


def extract_statements(vpath):
    """[(name, rocq_type)] for each theorem-like statement in a curated rocq.v.
    Binders are folded into a `forall`; type text is left as verbatim Rocq."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    out = []
    for s in translate.split_sentences(text):
        m = STMT_RE.match(s.strip())
        if not m:
            continue
        name, rest = m.group(1), m.group(2)
        parts = translate.split_top_level(rest, ":", maxsplit=1)  # binders : type
        if len(parts) != 2:
            raise ValueError(f"{name}: statement has no top-level ':'")
        binders, typ = (re.sub(r"\s+", " ", p).strip() for p in parts)
        out.append((name, f"forall {binders}, {typ}" if binders else typ))
    return out


def _run_coqc(coq, preamble, body, workdir):
    """Compile preamble+body as a temp .v; return (ok, one-line error message)."""
    fd, path = tempfile.mkstemp(suffix=".v", prefix="fidelity_", dir=workdir)
    with os.fdopen(fd, "w") as f:
        f.write("\n".join(preamble) + "\n" + body + "\n")
    try:
        try:
            proc = subprocess.run([coq, "-q", os.path.basename(path)],
                                  capture_output=True, text=True, cwd=workdir)
        except OSError as e:
            return False, f"could not run '{coq}': {e}"
        if proc.returncode == 0:
            return True, ""
        return False, re.sub(r"\s+", " ", (proc.stderr or proc.stdout).strip())
    finally:
        translate.clean_coqc_temp(path)


def qualify_ref(ref, file):
    """Prefix a stdlib ref with a library file, preserving any leading `@`:
    ('andb_diag', 'Stdlib.Bool.Bool') -> 'Stdlib.Bool.Bool.andb_diag'
    ('@conj', 'Corelib.Init.Logic')   -> '@Corelib.Init.Logic.conj'."""
    at = ref.startswith("@")
    return ("@" if at else "") + f"{file}.{ref[1:] if at else ref}"


def check_lemma(coq, preamble, entry, rocq_type, files, workdir):
    """Check one curated lemma against its stdlib counterpart, trying each of the
    module's `files` in turn.  Returns (status, ref, detail):
      match      convertible to <file>.<ref>          (ref = qualified name)
      symmetry   mirror equivalence verified          (ref = qualified name)
      MISMATCH   ref exists in a file but type differs
      ABSENT     ref exists in none of the files
      NO_STDLIB  map entry has no 'stdlib' ref
      ERROR      cannot run (no files / unknown relation)"""
    ref = entry.get("stdlib")
    if not ref:
        return "NO_STDLIB", None, "map entry has no 'stdlib' ref"
    if not files:
        return "ERROR", ref, "module has no 'module_files' association in the map"
    relation = entry.get("relation", "convertible")
    if relation not in ("convertible", "symmetry"):
        return "ERROR", ref, f"unknown relation '{relation}'"

    existed, last_msg = False, ""
    for file in files:
        qref = qualify_ref(ref, file)
        if relation == "convertible":
            body = f"Check ({qref} : {rocq_type})."
        else:
            body = f"Goal {rocq_type}.\nProof. intros; symmetry; apply {qref}. Qed."
        ok, msg = _run_coqc(coq, preamble, body, workdir)
        if ok:
            return ("match" if relation == "convertible" else "symmetry"), qref, file
        # Failed here: real MISMATCH if the name exists in this file, else keep looking.
        found, _ = _run_coqc(coq, preamble, f"Check {qref}.", workdir)
        if found:
            existed, last_msg = True, msg
    if existed:
        return "MISMATCH", ref, last_msg
    return "ABSENT", ref, f"'{ref}' is in none of the module's files ({', '.join(files)})"


def run(cfg, modules=None):
    """Check every curated lemma against its stdlib counterpart.  Returns 0 if all
    correspondences hold and every lemma is mapped; 1 on any failure."""
    corpus_dir, coq = cfg["corpus_dir"], cfg["coq_path"]
    spec = translate.load_stdlib_map(corpus_dir)
    preamble, mod_map = spec["preamble"], spec["modules"]
    module_files = spec.get("module_files", {})
    sel = modules or translate.corpus_modules(corpus_dir)

    tally = {k: 0 for k in ("match", "symmetry", "MISMATCH", "ABSENT",
                            "NO_STDLIB", "ERROR", "UNMAPPED", "STALE")}
    failures = []
    workdir = tempfile.mkdtemp(prefix="fidelity_")
    try:
        for mod in sel:
            vpath = os.path.join(corpus_dir, mod, "rocq.v")
            if not os.path.exists(vpath):
                print(f"  ! no such module: {mod}", file=sys.stderr)
                continue
            stmts = extract_statements(vpath)
            entries = mod_map.get(mod, {})
            files = module_files.get(mod, [])
            if not files:
                failures.append(f"{mod}: no 'module_files' entry in stdlib_map.json")
            print(f"\n{mod} ({len(stmts)} lemmas, files: {', '.join(files) or '—'}):")

            seen = set()
            for name, rocq_type in stmts:
                seen.add(name)
                if name not in entries:  # curated lemma with no map entry
                    tally["UNMAPPED"] += 1
                    failures.append(f"{mod}.{name}: not in stdlib_map.json")
                    print(f"  [UNMAPPED] {name}  (add it to stdlib_map.json)")
                    continue
                status, ref, detail = check_lemma(coq, preamble, entries[name],
                                                  rocq_type, files, workdir)
                tally[status] += 1
                if status == "match":
                    print(f"  [match   ] {name:18s} <- {ref}")
                elif status == "symmetry":
                    print(f"  [symmetry] {name:18s} <- {ref}  (mirror, up to eq_sym)")
                else:  # MISMATCH / ABSENT / NO_STDLIB / ERROR
                    print(f"  [{status:8s}] {name:18s}" + (f" <- {ref}" if ref else ""))
                    print(f"             {detail[:240]}")
                    failures.append(f"{mod}.{name} ({status}): {detail[:160]}")

            for name in entries:  # map entry with no curated lemma
                if name not in seen:
                    tally["STALE"] += 1
                    failures.append(f"{mod}.{name}: in stdlib_map.json but not in rocq.v")
                    print(f"  [STALE   ] {name}  (in map, absent from rocq.v)")
    finally:
        try:
            os.rmdir(workdir)
        except OSError:
            pass

    total = sum(tally.values()) - tally["STALE"]  # STALE lemmas aren't curated
    print(f"\n{total} curated lemmas: {tally['match']} convertible, "
          f"{tally['symmetry']} symmetry-variant, "
          f"{tally['MISMATCH']} mismatch, {tally['ABSENT']} absent-from-file, "
          f"{tally['NO_STDLIB']} without-stdlib-counterpart, "
          f"{tally['ERROR']} error, "
          f"{tally['UNMAPPED']} unmapped, {tally['STALE']} stale.")
    if failures:
        print("\nFIDELITY FAILURES:")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("\nAll curated statements correspond to their stdlib counterparts "
          "and belong to their module's standard-library file.")
    return 0
