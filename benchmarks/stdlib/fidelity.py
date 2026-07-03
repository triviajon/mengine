#!/usr/bin/env python3
"""Statement-level fidelity check: curated corpus lemma vs real Rocq stdlib.

The corpus `rocq.v` files are *hand-curated* re-statements of standard-library
lemmas (the real stdlib files don't translate to MEngine — see README §"Why
whole stdlib files don't translate").  That hand step is the one place a curated
statement could silently drift from the library lemma it claims to be.  This
module closes that gap by letting **Rocq's own kernel** compare the two, and it
additionally enforces that every curated lemma *belongs to* the standard-library
file its module claims — the ref is qualified with that file before the check, so
a name that only exists elsewhere (or nowhere) fails:

  - Each module maps (via the map's ``module_files``) to the stdlib file(s) its
    lemmas are drawn from — e.g. ``Bool`` -> ``Stdlib.Bool.Bool``, ``Nat`` ->
    ``Stdlib.Arith.PeanoNat``.  The ``<stdlib_ref>`` is qualified with a file
    (``andb_diag`` -> ``Stdlib.Bool.Bool.andb_diag``) so the check proves *both*
    that the lemma exists in the associated file and that its type matches.
  - For a lemma mapped to a stdlib counterpart, it emits
    ``Check (<file>.<stdlib_ref> : <curated statement>).`` — Rocq accepts it iff
    the qualified stdlib lemma's type is convertible to the curated statement's.
  - For a counterpart whose statement is the *mirror* of the curated one
    (``relation: symmetry`` — e.g. stdlib ``add_assoc`` is the other
    orientation), it emits ``Goal <curated>. Proof. intros; symmetry; apply
    <file>.<stdlib_ref>. Qed.``, verifying equivalence up to ``eq_sym``.

Every curated lemma MUST have a named stdlib counterpart in its module's file: the
corpus deliberately holds no bespoke theorems (ground computations, nested-
conjunction intros, …) that do not exist in the standard library, and a map entry
without a ``stdlib`` ref is a failure (``NO_STDLIB``), not a tolerated category.

The map (`corpus/stdlib_map.json`) is the single source of truth and must cover
every curated lemma; an unmapped lemma is a failure, so the check stays honest
as the corpus grows.  Nothing here is guessed — every ref/relation/file was
confirmed against the installed Rocq before being recorded (the translator's
"flag, never guess" principle, applied to the curation step).
"""

import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import translate  # noqa: E402  (local module; reuse its comment/sentence split)

STMT_RE = re.compile(r"^(?:" + translate.THEOREM_ALT +
                     r")\s+([A-Za-z_][A-Za-z0-9_']*)\s*(.*)$", re.S)


def extract_statements(vpath):
    """Ordered [(name, rocq_type_text)] for every theorem-like statement in a
    curated `rocq.v`.  The type text is verbatim Rocq surface syntax (notation,
    implicits intact) — exactly what we ascribe the stdlib lemma to."""
    with open(vpath) as f:
        text = translate.strip_comments(f.read())
    out = []
    for s in translate.split_sentences(text):
        m = STMT_RE.match(s.strip())
        if not m:
            continue
        name, rest = m.group(1), m.group(2)
        parts = translate.split_top_level(rest, ":", maxsplit=1)
        if len(parts) != 2:
            raise ValueError(f"{name}: statement has no top-level ':'")
        binders, typ = parts
        typ = re.sub(r"\s+", " ", typ).strip()
        binders = re.sub(r"\s+", " ", binders).strip()
        rocq_type = f"forall {binders}, {typ}" if binders else typ
        out.append((name, rocq_type))
    return out


def _run_coqc(coq, preamble, body, workdir):
    """Write preamble+body to a temp .v, run coqc, return (ok, message)."""
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
        msg = (proc.stderr or proc.stdout).strip()
        msg = re.sub(r"\s+", " ", msg)
        return False, msg
    finally:
        translate.clean_coqc_temp(path)


def qualify_ref(ref, file):
    """Fully-qualify a bare/`@`-prefixed stdlib ref with a library file path:
    ('andb_diag', 'Stdlib.Bool.Bool')     -> 'Stdlib.Bool.Bool.andb_diag',
    ('@conj', 'Corelib.Init.Logic')        -> '@Corelib.Init.Logic.conj',
    ('Nat.add_0_l', 'Stdlib.Arith.PeanoNat') -> 'Stdlib.Arith.PeanoNat.Nat.add_0_l'.
    Qualifying pins the ref to one file, so the Check both proves membership in
    that file and disambiguates names the library re-exports under several paths
    (e.g. a bare `eq_sym` also resolves as `Nat.eq_sym`)."""
    at = ref.startswith("@")
    bare = ref[1:] if at else ref
    return ("@" if at else "") + f"{file}.{bare}"


def check_lemma(coq, preamble, entry, rocq_type, files, workdir):
    """Run the Rocq-side check for one curated lemma against its stdlib
    counterpart, requiring the ref to live in one of the module's associated
    library `files`.  The ref is qualified with each file in turn, so a single
    `Check` proves *both* file-membership and convertibility.

    Returns (status, ref, detail) where status is one of:
      'match'     convertible to <file>.<ref> for some associated file,
      'symmetry'  verified the mirror equivalence against <file>.<ref>,
      'MISMATCH'  <ref> exists in an associated file but is NOT convertible,
      'ABSENT'    <ref> is defined in no associated file (the theorem does not
                  belong to this module's standard-library file),
      'NO_STDLIB' the map entry has no 'stdlib' ref (a bespoke theorem with no
                  standard-library counterpart — no longer permitted),
      'ERROR'     the check could not run (unknown relation, no files, …).
    The returned ref is the qualified name on success, the bare ref otherwise."""
    ref = entry.get("stdlib")
    if not ref:
        return "NO_STDLIB", None, (
            "map entry has no 'stdlib' ref; every corpus lemma must correspond to "
            "a named standard-library lemma")
    if not files:
        return "ERROR", ref, "module has no 'module_files' association in the map"
    relation = entry.get("relation", "convertible")
    if relation not in ("convertible", "symmetry"):
        return "ERROR", ref, f"unknown relation '{relation}'"
    existed, last_msg = False, ""
    for file in files:
        qref = qualify_ref(ref, file)
        if relation == "convertible":
            ok, msg = _run_coqc(coq, preamble, f"Check ({qref} : {rocq_type}).", workdir)
            if ok:
                return "match", qref, file
        else:  # symmetry
            body = (f"Goal {rocq_type}.\n"
                    f"Proof. intros; symmetry; apply {qref}. Qed.")
            ok, msg = _run_coqc(coq, preamble, body, workdir)
            if ok:
                return "symmetry", qref, file
        # Not convertible under this file — does the qualified name even exist
        # there?  If so it's a genuine type MISMATCH; if not, keep looking.
        found, _ = _run_coqc(coq, preamble, f"Check {qref}.", workdir)
        if found:
            existed, last_msg = True, msg
    if existed:
        return "MISMATCH", ref, last_msg
    return "ABSENT", ref, (f"'{ref}' is defined in none of the module's "
                           f"associated files ({', '.join(files)})")


def run(cfg, modules=None):
    """Check every curated lemma's statement against its stdlib counterpart.

    Returns 0 if every mapped correspondence holds and every curated lemma is
    mapped; 1 if any mismatch / unmapped lemma / stale map entry / coqc error."""
    corpus_dir = cfg["corpus_dir"]
    coq = cfg["coq_path"]
    spec = translate.load_stdlib_map(corpus_dir)
    preamble = spec["preamble"]
    mod_map = spec["modules"]
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
                if name not in entries:
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
                    print(f"  [symmetry] {name:18s} <- {ref}  "
                          f"(mirror; verified equivalent up to eq_sym)")
                else:  # MISMATCH / ABSENT / NO_STDLIB / ERROR
                    print(f"  [{status:8s}] {name:18s}" +
                          (f" <- {ref}" if ref else ""))
                    print(f"             {detail[:240]}")
                    failures.append(f"{mod}.{name} ({status}): {detail[:160]}")
            # Stale map entries: a name in the map with no curated lemma.
            for name in entries:
                if name not in seen:
                    tally["STALE"] += 1
                    failures.append(f"{mod}.{name}: in stdlib_map.json but not in rocq.v")
                    print(f"  [STALE   ] {name}  (in map, absent from rocq.v)")
    finally:
        try:
            os.rmdir(workdir)
        except OSError:
            pass

    total = sum(tally.values()) - tally["STALE"]
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
