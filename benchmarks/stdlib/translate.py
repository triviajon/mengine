#!/usr/bin/env python3
"""Mechanical Rocq (.v) -> MEngine (.me) translator for the stdlib benchmark.

This is a *lightweight* sentence-aware rewriter with a small recursive-descent
parser for the term sublanguage, NOT a full Coq elaborator.  Its guiding principle is
**flag, never guess**: any construct it cannot translate soundly is reported as
an `unhandled` item and (in unit mode) raises `Untranslatable`, so the unit is
excluded from Tier A rather than mistranslated.  A wrong translation that happened
to compile would silently corrupt the benchmark, which is the worst outcome.

Scope (Tier A): the computational/structural corners reachable by today's MEngine
+ compat prelude — Bool, ground Nat, the `le` order, and propositional logic.
Polymorphic list reasoning needs element-type inference and is deferred (Tier B).

Statement and definition *types* are always elaborated through Rocq (`Set Printing
All`), so a working `coqc`/`rocq` binary (``--coq``) is required; see
``rocq_elaborate``.  Terms inside definition bodies and proof tactics are still
translated from the surface source (they are not always elaborable).

Usage:
    translate.py unit.v              # emit MEngine source on stdout
    translate.py --report unit.v     # print handled/unhandled construct summary
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile


class Untranslatable(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


# ───────────────────────────── comment / sentence split ──────────────────────

def strip_comments(text):
    """Remove (* ... *) comments (nested), keeping newlines for line numbers."""
    out = []
    depth = 0
    i = 0
    n = len(text)
    while i < n:
        if text[i:i + 2] == "(*":
            depth += 1
            i += 2
        elif text[i:i + 2] == "*)" and depth > 0:
            depth -= 1
            i += 2
        elif depth > 0:
            out.append("\n" if text[i] == "\n" else " ")
            i += 1
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def split_sentences(text):
    """Split on '.' followed by whitespace/EOF, ignoring '..'/'...' and decimals."""
    sentences = []
    buf = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        buf.append(c)
        if c == ".":
            nxt = text[i + 1] if i + 1 < n else " "
            prev = text[i - 1] if i > 0 else " "
            is_ellipsis = nxt == "." or prev == "."
            is_decimal = prev.isdigit() and nxt.isdigit()
            if not is_ellipsis and not is_decimal and (nxt.isspace() or i + 1 == n):
                sentences.append("".join(buf).strip().rstrip("."))
                buf = []
        i += 1
    tail = "".join(buf).strip().rstrip(".")
    if tail:
        sentences.append(tail)
    return [s for s in sentences if s]


# ───────────────────────────────── term lexer ────────────────────────────────

TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<arrow>->)
  | (?P<fatarrow>=>)
  | (?P<sym>[(),:])
  | (?P<num>\d+)
  | (?P<ident>[A-Za-z_][A-Za-z0-9_']*)
""", re.VERBOSE)

KEYWORDS = {"forall", "fun", "match", "with", "end", "let", "in", "Prop",
            "Type", "Set"}


class Tok:
    def __init__(self, kind, val):
        self.kind = kind
        self.val = val

    def __repr__(self):
        return f"Tok({self.kind},{self.val!r})"


def lex_term(s):
    toks = []
    i = 0
    n = len(s)
    while i < n:
        m = TOKEN_RE.match(s, i)
        if not m:
            raise Untranslatable(f"unexpected character {s[i]!r} in term")
        i = m.end()
        kind = m.lastgroup
        if kind == "ws":
            continue
        val = m.group()
        if kind == "ident" and val in KEYWORDS:
            toks.append(Tok(val, val))
        elif kind == "ident":
            toks.append(Tok("ident", val))
        elif kind == "num":
            toks.append(Tok("num", val))
        elif kind == "arrow":
            toks.append(Tok("op", "->"))
        elif kind == "fatarrow":
            toks.append(Tok("=>", "=>"))
        elif kind == "sym":
            toks.append(Tok(val, val))
    return toks


# ───────────────────────────────── AST ───────────────────────────────────────
# Node forms (tuples):
#   ("var", name) | ("app", f, a) | ("forall", var, ty, body)
#   ("fun", var, ty, body) | ("arrow", a, b) | ("num", int)


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    def peek(self):
        return self.toks[self.pos] if self.pos < len(self.toks) else Tok("eof", None)

    def next(self):
        t = self.peek()
        self.pos += 1
        return t

    def expect(self, kind):
        t = self.next()
        if t.kind != kind:
            raise Untranslatable(f"expected {kind}, got {t.kind} {t.val!r}")
        return t

    # binders: parse `(x y : T)` groups or bare `x y` (then ',' or ':') for
    # forall/fun.  Returns list of (name, type_or_None).
    def parse_binders(self):
        binders = []
        while True:
            t = self.peek()
            if t.kind == "(":
                self.next()
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                if not names:
                    raise Untranslatable("empty binder group")
                ty = None
                if self.peek().kind == ":":
                    self.next()
                    ty = self.parse_expr()
                self.expect(")")
                for nm in names:
                    binders.append((nm, ty))
            elif t.kind == "ident":
                # bare names until ':' or ',' (untyped binders need a later ':')
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                ty = None
                if self.peek().kind == ":":
                    self.next()
                    ty = self.parse_expr()
                for nm in names:
                    binders.append((nm, ty))
                break
            else:
                break
        return binders

    def parse_atom(self):
        t = self.peek()
        if t.kind == "(":
            self.next()
            e = self.parse_expr()
            self.expect(")")
            return e
        if t.kind == "forall":
            self.next()
            binders = self.parse_binders()
            self.expect(",")
            body = self.parse_expr()
            node = body
            for nm, ty in reversed(binders):
                if ty is None:
                    raise Untranslatable("forall binder without a type annotation")
                node = ("forall", nm, ty, node)
            return node
        if t.kind == "fun":
            self.next()
            binders = self.parse_binders()
            self.expect("=>")
            body = self.parse_expr()
            node = body
            for nm, ty in reversed(binders):
                if ty is None:
                    raise Untranslatable("fun binder without a type annotation")
                node = ("fun", nm, ty, node)
            return node
        if t.kind in ("match", "let"):
            raise Untranslatable(f"'{t.kind}' in statement term (Tier B)")
        if t.kind == "ident":
            self.next()
            return ("var", t.val)
        if t.kind in ("Prop", "Type", "Set"):
            self.next()
            return ("var", "Prop" if t.kind == "Prop" else "Type")
        if t.kind == "num":
            self.next()
            return ("num", int(t.val))
        raise Untranslatable(f"unexpected token {t.kind} {t.val!r} in term")

    def parse_app(self):
        node = self.parse_atom()
        while self.peek().kind in ("ident", "(", "num", "Prop", "Type", "Set"):
            arg = self.parse_atom()
            node = ("app", node, arg)
        return node

    def parse_expr(self):
        left = self.parse_app()
        if self.peek().kind == "op" and self.peek().val == "->":
            self.next()
            return ("arrow", left, self.parse_expr())  # '->' is right-associative
        return left


def parse_term(s):
    toks = lex_term(s)
    p = Parser(toks)
    e = p.parse_expr()
    if p.peek().kind != "eof":
        raise Untranslatable(f"trailing tokens after term: {p.peek().val!r}")
    return e


# ─────────────────── elaborated-form parser (Set Printing All) ────────────────
#
# Each statement's type is obtained from Rocq's `Set Printing All` output rather
# than the surface source (see ``rocq_elaborate``).  That
# form is *notation-free and fully explicit*: every implicit argument is shown,
# `@head` marks an application with all implicits supplied, numerals are expanded
# to `O`/`S`, and the only qualified heads are the `Nat.*` arithmetic ops.
# Translating this form needs no infix-notation table and no eq-type synthesis:
# every operator prints as a plain prefix application (`@eq T x y`, not `x = y`),
# and the implicit type arguments (e.g. that `T`, or the element type of a list)
# are already present — which is exactly why statements and definition types are
# routed through Rocq first.  See README.

# Qualified heads that `Set Printing All` emits, mapped to compat-prelude names.
# Any *other* dotted (qualified) head is flagged, never guessed.
NAME_MAP = {
    "Nat.add": "add", "Nat.mul": "mul", "Nat.sub": "sub",
    "Nat.eqb": "eqb", "Nat.leb": "leb", "Nat.ltb": "ltb",
    "Nat.pred": "pred", "Nat.max": "max", "Nat.min": "min",
}

ELAB_TOKEN_RE = re.compile(r"""
    (?P<ws>\s+)
  | (?P<arrow>->)
  | (?P<fatarrow>=>)
  | (?P<at>@)
  | (?P<sym>[(),:])
  | (?P<qident>[A-Za-z_][A-Za-z0-9_']*(?:\.[A-Za-z_][A-Za-z0-9_']*)*)
""", re.VERBOSE)

ELAB_KEYWORDS = {"forall", "fun", "match", "with", "end", "return", "in",
                 "let", "as", "Prop", "Type", "Set", "SProp"}


def lex_elab(s):
    toks = []
    i, n = 0, len(s)
    while i < n:
        m = ELAB_TOKEN_RE.match(s, i)
        if not m:
            raise Untranslatable(f"unexpected character {s[i]!r} in elaborated term")
        i = m.end()
        kind = m.lastgroup
        if kind == "ws":
            continue
        val = m.group()
        if kind == "arrow":
            toks.append(Tok("op", "->"))
        elif kind == "fatarrow":
            toks.append(Tok("=>", "=>"))
        elif kind == "at":
            toks.append(Tok("@", "@"))
        elif kind == "sym":
            toks.append(Tok(val, val))
        elif kind == "qident":
            toks.append(Tok(val if val in ELAB_KEYWORDS else "ident", val))
    return toks


def _map_name(name):
    if name in NAME_MAP:
        return NAME_MAP[name]
    if "." in name:
        raise Untranslatable(f"unmapped qualified name '{name}' in elaborated term")
    return name


class ElabParser:
    """Recursive-descent parser for `Set Printing All` term syntax.  Emits the
    same AST tuples (`var`/`app`/`forall`/`fun`/`arrow`) that ``emit`` renders."""

    def __init__(self, toks):
        self.toks = toks
        self.pos = 0

    def peek(self):
        return self.toks[self.pos] if self.pos < len(self.toks) else Tok("eof", None)

    def next(self):
        t = self.peek()
        self.pos += 1
        return t

    def expect(self, kind):
        t = self.next()
        if t.kind != kind:
            raise Untranslatable(f"elaborated: expected {kind}, got {t.kind} {t.val!r}")
        return t

    def parse_binders(self):
        """Parse `(x y : T) (z : U)` groups or a single bare `x : T`."""
        binders = []
        while True:
            t = self.peek()
            if t.kind == "(":
                self.next()
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                if not names:
                    raise Untranslatable("elaborated: empty binder group")
                self.expect(":")
                ty = self.parse_expr()
                self.expect(")")
                for nm in names:
                    binders.append((nm, ty))
            elif t.kind == "ident":
                names = []
                while self.peek().kind == "ident":
                    names.append(self.next().val)
                self.expect(":")
                ty = self.parse_expr()
                for nm in names:
                    binders.append((nm, ty))
                break
            else:
                break
        return binders

    def parse_atom(self):
        t = self.peek()
        if t.kind == "@":
            self.next()
            return ("var", _map_name(self.expect("ident").val))
        if t.kind == "(":
            self.next()
            e = self.parse_expr()
            self.expect(")")
            return e
        if t.kind == "ident":
            self.next()
            return ("var", _map_name(t.val))
        if t.kind in ("Prop", "Type", "Set", "SProp"):
            self.next()
            return ("var", "Type" if t.kind == "Type" else "Prop")
        if t.kind in ("match", "let", "fix"):
            raise Untranslatable(f"'{t.kind}' in elaborated statement type (Tier B)")
        raise Untranslatable(f"elaborated: unexpected token {t.kind} {t.val!r}")

    def parse_app(self):
        node = self.parse_atom()
        while self.peek().kind in ("ident", "@", "(", "Prop", "Type", "Set", "SProp"):
            node = ("app", node, self.parse_atom())
        return node

    def parse_expr(self):
        t = self.peek()
        if t.kind == "forall":
            self.next()
            binders = self.parse_binders()
            self.expect(",")
            node = self.parse_expr()
            for nm, ty in reversed(binders):
                node = ("forall", nm, ty, node)
            return node
        if t.kind == "fun":
            self.next()
            binders = self.parse_binders()
            self.expect("=>")
            node = self.parse_expr()
            for nm, ty in reversed(binders):
                node = ("fun", nm, ty, node)
            return node
        left = self.parse_app()
        if self.peek().kind == "op" and self.peek().val == "->":
            self.next()
            return ("arrow", left, self.parse_expr())  # '->' is right-associative
        return left


def parse_elab(s):
    p = ElabParser(lex_elab(s))
    e = p.parse_expr()
    if p.peek().kind != "eof":
        raise Untranslatable(f"elaborated: trailing tokens {p.peek().val!r}")
    return e


def translate_elab_type(type_str):
    """Render a `Set Printing All` type string as MEngine prefix syntax."""
    return emit(parse_elab(type_str))


# ───────────────────────────── pretty printer ────────────────────────────────

def emit(node):
    """Render an AST node as fully-parenthesized MEngine prefix syntax."""
    kind = node[0]
    if kind == "var":
        return node[1]
    if kind == "num":
        out = "O"
        for _ in range(node[1]):
            out = f"(S {out})"
        return out
    if kind == "app":
        f = emit(node[1])
        a = emit(node[2])
        # A binder-headed argument (fun/forall/arrow) must be parenthesized:
        # MEngine's parser will not accept a bare `fun …`/`forall …` as an
        # application argument (e.g. the predicate of `ex A (fun y => …)`).
        if node[2][0] in ("fun", "forall", "arrow"):
            a = f"({a})"
        return f"({f} {a})"
    if kind == "arrow":
        a = emit(node[1])
        b = emit(node[2])
        return f"forall (_ : {a}), {b}"
    if kind == "forall":
        nm, ty, body = node[1], node[2], node[3]
        return f"forall ({nm} : {emit(ty)}), {emit(body)}"
    if kind == "fun":
        nm, ty, body = node[1], node[2], node[3]
        return f"fun ({nm} : {emit(ty)}) => {emit(body)}"
    raise Untranslatable(f"cannot emit node {kind}")


def translate_term(src):
    return emit(parse_term(src.strip()))


# ──────────────────────────── command translation ────────────────────────────

DROP_PREFIXES = ("Require", "From", "Import", "Export", "Open", "Close", "Set",
                 "Unset", "Hint", "Arguments", "Local", "Global", "#[", "Scope",
                 "Declare", "Generalizable", "Print", "Check", "Search",
                 "Comments", "Section", "End", "Module", "Include")

PROOF_FRAMING = ("Proof", "Qed", "Defined", "Admitted", "Abort")


def translate_definition(sentence, report, elab):
    """Definition/Lemma/Theorem/Example name [binders] : type [:= body].

    The statement type is always taken from ``elab`` — Rocq's `Set Printing All`
    output — rather than parsed from the surface source; a Definition *body* is
    still translated from the surface (terms are not always elaborable)."""
    m = re.match(r"^(Definition|Lemma|Theorem|Example|Corollary|Fact|Remark|Proposition)\s+"
                 r"([A-Za-z_][A-Za-z0-9_']*)\s*(.*)$", sentence, re.S)
    if not m:
        raise Untranslatable("unrecognized definition form")
    kw, name, rest = m.group(1), m.group(2), m.group(3)
    is_thm = kw in ("Lemma", "Theorem", "Example", "Corollary", "Fact",
                    "Remark", "Proposition")
    if name not in elab:
        raise Untranslatable(f"no elaborated type for '{name}'")

    # Separate optional binders + ': type' + optional ':= body'.
    body = None
    if ":=" in rest:
        head, body = rest.split(":=", 1)
    else:
        head = rest

    type_out = translate_elab_type(elab[name])

    if is_thm:
        report.add_handled(kw)
        return f"Theorem {name} : {type_out}."
    # Definition with a body.
    if body is None:
        report.add_handled("Definition(no body)")
        return f"Axiom {name} : {type_out}."
    # The elaborated type is fully explicit, but the body is not always elaborable,
    # so it is translated from the surface source, wrapped in the surface binders.
    binders_src = head.split(":", 1)[0] if ":" in head else head
    binders = _parse_binder_src(binders_src) if binders_src.strip() else []
    body_full = parse_term(body.strip())
    for nm, ty in reversed(binders):
        body_full = ("fun", nm, ty, body_full)
    report.add_handled("Definition")
    return f"Definition {name} : {type_out} := {emit(body_full)}."


def _parse_binder_src(src):
    src = src.strip()
    if not src:
        return []
    p = Parser(lex_term(src))
    binders = p.parse_binders()
    if p.peek().kind != "eof":
        raise Untranslatable("could not parse definition binders")
    for nm, ty in binders:
        if ty is None:
            raise Untranslatable(f"binder '{nm}' lacks a type annotation")
    return binders


def translate_axiom(sentence, report, elab):
    m = re.match(r"^(Axiom|Parameter|Conjecture|Variable|Hypothesis)\s+"
                 r"([A-Za-z_][A-Za-z0-9_']*)\s*:\s*(.*)$", sentence, re.S)
    if not m:
        raise Untranslatable("unrecognized axiom form")
    name = m.group(2)
    if name not in elab:
        raise Untranslatable(f"no elaborated type for '{name}'")
    report.add_handled("Axiom")
    return f"Axiom {name} : {translate_elab_type(elab[name])}."


def is_dropped(sentence):
    for p in DROP_PREFIXES:
        if sentence.startswith(p):
            return True
    return False


def is_framing(sentence):
    word = re.match(r"^([A-Za-z]+)", sentence)
    return bool(word and word.group(1) in PROOF_FRAMING)


# ──────────────────────────── tactic translation ─────────────────────────────
#
# Token-level mapping of a single Rocq tactic atom (no ';') to MEngine.  Unknown
# tactics hard-stop (Untranslatable) so the unit leaves Tier A.

# Inductive data for the induction/destruct scaffold (compat-prelude types).
# `params` is the number of leading type parameters the eliminator takes (1 for
# `list A`, 0 for nat/bool); `cases` lists, per constructor in declaration order,
# the recursive flag of each *argument* (True = recursive occurrence, carrying an
# induction hypothesis in <T>_ind; the parameter slot is not an argument here).
INDUCTIVES = {
    "bool": {"ind": "bool_ind", "params": 0, "cases": [("true", []), ("false", [])]},
    "nat":  {"ind": "nat_ind",  "params": 0, "cases": [("O", []), ("S", [True])]},
    "list": {"ind": "list_ind", "params": 1, "cases": [("nil", []), ("cons", [False, True])]},
}

DIRECT_TACTICS = {
    "reflexivity", "assumption", "split", "left", "right", "auto",
    "constructor", "simpl", "symmetry", "trivial", "easy", "idtac",
    # f_equal is emulated in the compat prelude (a single-layer structural
    # congruence on Bad_App_Congruence — see compat/stdlib_compat.me); the
    # translator passes it through like the other compat-prelude tactics.
    "f_equal",
}

UNSUPPORTED = {
    "lia", "ring", "omega", "nia", "field", "auto with", "eauto", "inversion",
    "congruence", "unfold", "fold", "replace", "generalize",
    "revert", "case", "pose", "set", "specialize", "discriminate", "injection",
    "contradiction", "exfalso", "cbn", "red", "change", "rename", "clear",
    "remember", "induction'", "dependent", "functional", "decide", "destruct'",
}


def translate_tactic_atom(atom, report):
    atom = atom.strip()
    if not atom:
        return None
    head = re.match(r"^([A-Za-z_][A-Za-z0-9_']*)", atom)
    name = head.group(1) if head else None

    if name in UNSUPPORTED:
        raise Untranslatable(f"unsupported tactic '{name}'")

    if name == "repeat":
        # `repeat t` — MEngine has the same combinator (prelude `intros := repeat
        # intro`).  Rocq binds `repeat` tighter than `;`, so the body is a single
        # atom (`repeat split; assumption` is `(repeat split); assumption`, split
        # at the sentence level before this point).
        inner = atom[len("repeat"):].strip()
        if not inner:
            raise Untranslatable("bare 'repeat'")
        t = translate_tactic_atom(inner, report)
        if t is None or "." in t or ";" in t:
            raise Untranslatable("repeat of a non-atomic tactic")
        report.add_handled("tac:repeat")
        return f"repeat {t}"

    if name in DIRECT_TACTICS:
        rest = atom[len(name):].strip()
        if rest:
            raise Untranslatable(f"tactic '{name}' with unexpected arguments")
        report.add_handled(f"tac:{name}")
        return name

    if name in ("intro", "intros"):
        args = atom[len(name):].strip()
        report.add_handled("tac:intro")
        if not args:
            return "intros" if name == "intros" else "intro"
        names = args.split()
        return " ".join(f"intro {nm}." for nm in names).rstrip(".")

    if name in ("apply", "eapply"):
        arg = atom[len(name):].strip()
        if " in " in f" {arg} " or arg.endswith(" in"):
            raise Untranslatable("apply ... in H (Tier B)")
        report.add_handled(f"tac:{name}")
        return f"{name} ({translate_term(arg)})"

    if name == "exact":
        arg = atom[len("exact"):].strip()
        report.add_handled("tac:exact")
        return f"exact ({translate_term(arg)})"

    if name in ("exists",):
        arg = atom[len("exists"):].strip()
        report.add_handled("tac:exists")
        return f"exists ({translate_term(arg)})"

    if name == "rewrite":
        rest = atom[len("rewrite"):].strip()
        if rest.startswith("<-"):
            raise Untranslatable("rewrite <- (Tier B: builtin rewrite is forward-only)")
        if " in " in f" {rest} ":
            raise Untranslatable("rewrite ... in H (Tier B)")
        report.add_handled("tac:rewrite")
        # Builtin C rewrite (Leibniz eq).  Routed through the kernel rewrite engine
        # rather than the scripted `rewrite_s`, which cannot read the equality off an
        # induction hypothesis whose type is the eliminator's beta-redex `(motive) x`.
        return f"rewrite {translate_term(rest)} with eq"

    if name in ("now",):
        # now tac  ≈  tac; easy.  Bare 'now' is unusual; treat 'now' alone as easy.
        rest = atom[len("now"):].strip()
        report.add_handled("tac:now")
        if not rest:
            return "easy"
        inner = translate_tactic_atom(rest, report)
        return f"{inner}; easy"

    raise Untranslatable(f"unknown tactic '{name or atom}'")


def translate_tactic_sentence(sentence, report):
    """Translate one Rocq tactic sentence (may contain ';') -> MEngine, '.'-ended."""
    parts = _split_semicolons(sentence)
    out = []
    for part in parts:
        t = translate_tactic_atom(part, report)
        if t is not None:
            out.append(t)
    if not out:
        return ""
    return "; ".join(out) + "."


def _split_semicolons(s):
    """Split on top-level ';' (not inside brackets/parens)."""
    parts = []
    depth = 0
    buf = []
    for c in s:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == ";" and depth == 0:
            parts.append("".join(buf))
            buf = []
        else:
            buf.append(c)
    parts.append("".join(buf))
    return [p.strip() for p in parts if p.strip()]


# ──────────────────────────── proof translation ──────────────────────────────

def parse_statement_leading_binder(type_out):
    """From an emitted 'forall (x : T), Body' string, return (x, T, Body) or None.

    The binder type T may itself contain commas / nested parentheses — e.g. a
    function-typed binder `f : forall (_ : A), B` — so the binder's closing `)`
    is found by matching parentheses from the `(` after `forall `, rather than
    by a comma-naive regex (which would stop inside T)."""
    prefix = "forall ("
    if not type_out.startswith(prefix):
        return None
    depth = 0
    i = len("forall ")  # index of the binder's opening '('
    start = i
    while i < len(type_out):
        c = type_out[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                break
        i += 1
    if depth != 0 or i >= len(type_out):
        return None
    inner = type_out[start + 1:i]   # 'x : T'
    rest = type_out[i + 1:]
    if not rest.startswith(", "):
        return None
    body = rest[2:].strip()
    ci = inner.find(" : ")
    if ci < 0:
        return None
    name = inner[:ci].strip()
    ty = inner[ci + 3:].strip()
    if not re.match(r"^[A-Za-z_][A-Za-z0-9_']*$", name):
        return None
    return name, ty, body


def translate_proof(proof_sentences, stmt_type_out, report):
    """Translate a list of Rocq proof tactic-sentences into MEngine proof lines.

    Handles two shapes:
      (A) straight-line tactic sequence (no induction/destruct);
      (B) a leading `induction x` / `destruct x` on the goal's leading binder,
          with bullet- or position-segmented cases (nat/bool only).
    Anything else is flagged (Untranslatable)."""
    # Drop proof framing sentences.
    body = [s for s in proof_sentences if not is_framing(s)]
    if not body:
        raise Untranslatable("empty proof")

    first = body[0].strip()
    # The induction/destruct line may carry a uniform semicolon tail
    # (`induction b; reflexivity`) — a chain applied to *every* resulting
    # subgoal, the stdlib's idiom for case-splits all cases close the same way
    # (e.g. `destr_bool` ≈ `destruct b; simpl; reflexivity`).  It is only sound
    # when no case needs per-case intros, so `_translate_induction` restricts it
    # to all-nullary inductives (bool); anything else is flagged.
    m = re.match(r"^(induction|destruct)\s+([A-Za-z_][A-Za-z0-9_']*)"
                 r"\s*(?:as\s+(\[.*?\]))?\s*(?:;\s*(.+))?$", first, re.S)
    if m:
        return _translate_induction(m.group(1), m.group(2), m.group(3), body[1:],
                                    stmt_type_out, report,
                                    uniform_tail=m.group(4))

    # Shape (A): straight-line.  Forbid any later induction/destruct.
    for s in body:
        if re.match(r"^(induction|destruct)\b", s.strip()):
            raise Untranslatable("induction/destruct not as first tactic (Tier B)")
    lines = []
    for s in body:
        t = translate_tactic_sentence(s, report)
        if t:
            lines.append(t)
    return lines


def _segment_cases(case_sentences, ncases):
    """Segment proof remainder into per-case sentence lists, by '-' bullets or
    (fallback) one sentence per case."""
    bulleted = any(s.lstrip().startswith(("-", "+", "*")) for s in case_sentences)
    if bulleted:
        cases = []
        cur = None
        for s in case_sentences:
            st = s.lstrip()
            if st.startswith(("-", "+", "*")):
                if cur is not None:
                    cases.append(cur)
                cur = [st.lstrip("-+* ").strip()]
            else:
                if cur is None:
                    raise Untranslatable("proof text before first bullet")
                cur.append(s)
        if cur is not None:
            cases.append(cur)
        return cases
    # Fallback: require exactly one sentence per case.
    if len(case_sentences) != ncases:
        raise Untranslatable(
            f"cannot segment {len(case_sentences)} sentences into {ncases} cases "
            "(use '-' bullets)")
    return [[s] for s in case_sentences]


def _inductive_head_and_params(ty):
    """Split an emitted inductive type like '(list A)' or 'nat' into
    (head, params_string): ('list', 'A') or ('nat', '')."""
    t = ty.strip()
    if t.startswith("(") and t.endswith(")"):
        inner = t[1:-1].strip()
        depth = 0
        balanced = True
        for ch in inner:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth < 0:
                    balanced = False
                    break
        if balanced and depth == 0:
            t = inner
    parts = t.split(None, 1)
    head = parts[0]
    params = parts[1].strip() if len(parts) > 1 else ""
    return head, params


def _parse_as_clause(as_clause, ncases):
    """Parse `as [ | x l IHl ]` into a list of per-constructor name lists.
    Returns e.g. [[], ['x', 'l', 'IHl']]; or None if no clause."""
    if as_clause is None:
        return None
    inner = as_clause.strip()
    if not (inner.startswith("[") and inner.endswith("]")):
        raise Untranslatable("malformed `as` intro-pattern")
    inner = inner[1:-1]
    groups = [g.split() for g in inner.split("|")]
    if len(groups) != ncases:
        raise Untranslatable(
            f"`as` pattern has {len(groups)} cases, expected {ncases}")
    return groups


def _case_intro_lines(kind, var, rec_flags, names):
    """intro lines for one constructor case.

    The eliminator is always the *recursive* `<T>_ind`, whose step case binds an
    induction hypothesis (`P <rec-arg>`) for every recursive argument.  So each
    case must introduce: every constructor argument in order, then one
    hypothesis per recursive argument — for *both* `induction` and `destruct`,
    because both go through `<T>_ind` (MEngine generates no non-recursive
    `<T>_rec`/case principle).  They differ only in how that hypothesis is
    handled:

      - `induction` names it (from the `as` clause, else `IH<var>`) so the case
        body can use it.
      - `destruct` has no induction hypothesis in Rocq; here it is an artifact
        of emulating case analysis with `<T>_ind`, so it is discarded with a
        bare `intro.` (which still exposes `P (ctor ...)`), and the `as` clause
        names only the constructor arguments.  The resulting term is the same
        proof `induction` would build with the IH left unused — a full,
        Coq-checkable term — so this stays within flag-never-guess.

    With an explicit `as` group the argument names come verbatim; otherwise the
    arguments default to Rocq's auto-naming (reuse the variable for the single
    recursive argument), which is only well-defined when the constructor has no
    non-recursive arguments."""
    n_rec = sum(1 for f in rec_flags if f)
    if names is not None:
        # induction `as` lists args + IHs; destruct `as` lists only the args.
        n_named_ih = n_rec if kind == "induction" else 0
        if len(names) != len(rec_flags) + n_named_ih:
            raise Untranslatable(
                f"`as` case lists {len(names)} names, expected "
                f"{len(rec_flags) + n_named_ih}")
        lines = [f"intro {nm}." for nm in names]
        if kind != "induction":
            lines += ["intro." for _ in range(n_rec)]  # discard the IH(s)
        return lines
    if any(not f for f in rec_flags):
        raise Untranslatable(
            "induction/destruct without `as` on a constructor with "
            "non-recursive arguments (name them with `as [...]`)")
    lines = [f"intro {var}." for _ in rec_flags]
    if kind == "induction":
        lines += [f"intro IH{var}." for f in rec_flags if f]
    else:
        lines += ["intro." for f in rec_flags if f]  # discard the IH(s)
    return lines


def _translate_induction(kind, var, as_clause, remainder, stmt_type_out, report,
                         uniform_tail=None):
    # Peel leading binders, introducing each, until we reach the induction
    # variable.  Binders before it (e.g. the type parameter A of `list A`) are
    # introduced and then supplied to the eliminator via the variable's own type.
    rest = stmt_type_out
    intros = []
    var_ty = None
    body = None
    while True:
        lb = parse_statement_leading_binder(rest)
        if lb is None:
            raise Untranslatable(
                f"{kind} variable '{var}' is not a leading forall binder (Tier B)")
        name, ty, inner = lb
        intros.append(name)
        if name == var:
            var_ty, body = ty, inner
            break
        rest = inner

    head, params = _inductive_head_and_params(var_ty)
    if head not in INDUCTIVES:
        raise Untranslatable(f"{kind} on type '{head}' not supported")
    info = INDUCTIVES[head]
    if info["params"] and not params:
        raise Untranslatable(f"{kind} on '{head}' is missing its type parameter")

    motive = f"fun ({var} : {var_ty}) => {body}"
    param_prefix = f"{params} " if params else ""
    lines = [f"intro {b}." for b in intros]

    if uniform_tail is not None:
        # `induction x; t1; t2` — Rocq runs the chain on every subgoal of the
        # eliminator.  MEngine has `;` too, but its cross-goal form
        # (`apply (<T>_ind ...); t1; t2`) normalizes the sibling case goals
        # before either is closed, and `simpl`/`cbv` over the shared motive then
        # corrupts the not-yet-solved case.  So the chain is instead replayed
        # *per case, sequentially* — `apply (...)`, then `t1; t2` once for each
        # constructor — which yields the same proof term without the cross-goal
        # hazard.  Sound only when no case binds constructor arguments / an IH
        # (otherwise the chain would hit an unintroduced `forall`), so restrict
        # to all-nullary inductives (bool).
        if as_clause is not None:
            raise Untranslatable("uniform `;` tail with an `as` clause")
        if remainder:
            raise Untranslatable("uniform `;` tail followed by more tactics")
        if any(rec_flags for _ctor, rec_flags in info["cases"]):
            raise Untranslatable(
                f"uniform `;` tail on '{head}' (a constructor takes arguments; "
                "the chain cannot introduce them per case)")
        report.add_handled(f"tac:{kind}")
        tail_atoms = []
        for atom in _split_semicolons(uniform_tail):
            t = translate_tactic_atom(atom, report)
            if t is None:
                continue
            if "." in t:
                raise Untranslatable(
                    f"uniform `;` tail step '{atom}' is not a single tactic")
            tail_atoms.append(t)
        lines.append(f"apply ({info['ind']} {param_prefix}({motive})).")
        case_tail = "; ".join(tail_atoms) + "."
        for _case in info["cases"]:
            lines.append(case_tail)
        return lines

    lines.append(f"apply ({info['ind']} {param_prefix}({motive})).")

    names_per_case = _parse_as_clause(as_clause, len(info["cases"]))
    cases = _segment_cases(remainder, len(info["cases"]))
    if len(cases) != len(info["cases"]):
        raise Untranslatable(
            f"{kind}: {len(cases)} cases provided, expected {len(info['cases'])}")

    report.add_handled(f"tac:{kind}")
    for idx, ((ctor, rec_flags), case_body) in enumerate(zip(info["cases"], cases)):
        names = names_per_case[idx] if names_per_case is not None else None
        # The constructor args and IHs are introduced *first*; the `simpl` that
        # reduces the eliminator's `motive (ctor ...)` redex (MEngine does not
        # beta-reduce it automatically as Rocq does) must come after those intros.
        case_lines = _case_intro_lines(kind, var, rec_flags, names)
        case_lines.append("simpl.")
        for s in case_body:
            t = translate_tactic_sentence(s, report)
            if t:
                case_lines.append(t)
        lines.extend(case_lines)
    return lines


# ─────────────────────────────── report ──────────────────────────────────────

class Report:
    def __init__(self):
        self.handled = {}
        self.unhandled = []

    def add_handled(self, key):
        self.handled[key] = self.handled.get(key, 0) + 1

    def add_unhandled(self, reason):
        self.unhandled.append(reason)

    def ok(self):
        return not self.unhandled


# ─────────────────────────── unit-level translation ──────────────────────────

def translate_unit(text, report, elab):
    """Translate a whole .v unit to a MEngine source string, or raise.

    ``elab`` maps statement names to their `Set Printing All` types (see
    ``rocq_elaborate``); statement types are taken from there rather than parsed
    from the surface source."""
    text = strip_comments(text)
    sentences = split_sentences(text)

    out_lines = []
    i = 0
    n = len(sentences)
    while i < n:
        s = sentences[i].strip()
        if not s:
            i += 1
            continue
        word = re.match(r"^([A-Za-z_#\[]+)", s)
        kw = word.group(1) if word else ""

        if is_dropped(s):
            i += 1
            continue
        if kw in ("Inductive", "Fixpoint", "CoFixpoint"):
            raise Untranslatable(f"'{kw}' in unit (define it in the compat prelude)")
        # Separate each top-level item (statement + proof) with a blank line.
        if out_lines:
            out_lines.append("")
        if kw in ("Axiom", "Parameter", "Conjecture", "Variable", "Hypothesis"):
            out_lines.append(translate_axiom(s, report, elab))
            i += 1
            continue
        if kw in ("Definition",):
            out_lines.append(translate_definition(s, report, elab))
            i += 1
            continue
        if kw in ("Lemma", "Theorem", "Example", "Corollary", "Fact", "Remark",
                  "Proposition"):
            stmt = translate_definition(s, report, elab)
            out_lines.append(stmt)
            # Collect the proof: subsequent sentences until Qed/Defined/Admitted/Abort.
            type_out = stmt[len(f"Theorem "):].split(" : ", 1)[1].rstrip(".")
            proof = []
            i += 1
            while i < n:
                ps = sentences[i].strip()
                proof.append(ps)
                w = re.match(r"^([A-Za-z]+)", ps)
                if w and w.group(1) in ("Qed", "Defined", "Admitted", "Abort"):
                    break
                i += 1
            if any(re.match(r"^Admitted", p) or re.match(r"^Abort", p) for p in proof):
                raise Untranslatable("proof is Admitted/Abort")
            proof_lines = translate_proof(proof, type_out, report)
            out_lines.extend(proof_lines)
            i += 1
            continue
        # Unknown command.
        raise Untranslatable(f"unrecognized command '{kw}'")

    return "\n".join(out_lines) + "\n"


# ─────────────────────── Rocq elaboration (Set Printing All) ──────────────────
#
# Obtain the fully-explicit, notation-free type of each top-level statement by
# replaying the unit through Rocq with `Set Printing All` and a `Check` per name.
# This is what lets eq/list statements translate without surface type synthesis
# (the implicit type arguments are printed for us).  Failure (the unit does not
# compile, or a name is missing) is reported as Untranslatable — never guessed.

STMT_DECL_RE = re.compile(
    r"^(Definition|Lemma|Theorem|Example|Corollary|Fact|Remark|Proposition"
    r"|Axiom|Parameter|Conjecture)\s+([A-Za-z_][A-Za-z0-9_']*)")


def statement_names(text):
    """Ordered, de-duplicated names of every elaboratable top-level statement."""
    names, seen = [], set()
    for s in split_sentences(strip_comments(text)):
        m = STMT_DECL_RE.match(s.strip())
        if m and m.group(2) not in seen:
            seen.add(m.group(2))
            names.append(m.group(2))
    return names


def _parse_check_output(stdout, names):
    """Parse `Check`/`Print` output into {name: type-string}.

    Each `Check name.` prints the name flush-left on its own line, then the type
    on the following line(s) prefixed `     : ` (continuations are indented).  We
    set the print width effectively unbounded, so a forall/application/arrow type
    lands on a single line; we still join any continuations defensively."""
    name_set = set(names)
    out = {}
    lines = stdout.splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i]
        if ln and not ln[0].isspace() and ln.strip() in name_set and ln.strip() not in out:
            head = ln.strip()
            buf, j = [], i + 1
            while j < len(lines):
                cont = lines[j]
                if cont.strip() == "" or (cont and not cont[0].isspace()):
                    break
                buf.append(cont.strip())
                j += 1
            joined = " ".join(buf).strip()
            if joined.startswith(":"):
                joined = joined[1:].strip()
            out[head] = joined
            i = j
        else:
            i += 1
    missing = [n for n in names if n not in out]
    if missing:
        raise Untranslatable(f"rocq elaboration produced no type for {missing}")
    return out


def rocq_elaborate(text, vpath, coq_path, names):
    """Return {name: elaborated-type-string} via `coqc` + `Set Printing All`."""
    if not names:
        return {}
    unit_dir = os.path.dirname(os.path.abspath(vpath))
    appendix = ["", "Set Printing All.",
                "Set Printing Width 2000000000.",
                "Set Printing Depth 2000000000."]
    appendix += [f"Check {nm}." for nm in names]
    fd, tmp = tempfile.mkstemp(suffix=".v", prefix="elab_", dir=unit_dir)
    base = os.path.splitext(tmp)[0]
    try:
        with os.fdopen(fd, "w") as f:
            f.write(text.rstrip() + "\n" + "\n".join(appendix) + "\n")
        try:
            proc = subprocess.run([coq_path, "-q", os.path.basename(tmp)],
                                  capture_output=True, text=True, cwd=unit_dir)
        except OSError as e:
            raise Untranslatable(f"could not run '{coq_path}': {e}")
        if proc.returncode != 0:
            msg = (proc.stderr or proc.stdout).strip().replace("\n", " ")
            raise Untranslatable(f"rocq elaboration failed: {msg[:200]}")
        return _parse_check_output(proc.stdout, names)
    finally:
        for ext in (".v", ".vo", ".vok", ".vos", ".glob"):
            p = base + ext
            if os.path.exists(p):
                try:
                    os.remove(p)
                except OSError:
                    pass
        aux = os.path.join(unit_dir, "." + os.path.basename(base) + ".aux")
        if os.path.exists(aux):
            try:
                os.remove(aux)
            except OSError:
                pass


# ─────────────────────────────── statement digest ────────────────────────────

def statement_digests(mengine_src):
    """Normalized digest of each Theorem statement, for correspondence checking."""
    digs = []
    for line in mengine_src.splitlines():
        m = re.match(r"^Theorem\s+(\S+)\s*:\s*(.*)\.$", line)
        if m:
            norm = re.sub(r"\s+", " ", m.group(2)).strip()
            digs.append((m.group(1), norm))
    return digs


# ─────────────────────────────────── main ────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(description="Rocq .v -> MEngine .me translator")
    ap.add_argument("input", help="input .v file")
    ap.add_argument("--report", action="store_true", help="report handled/unhandled")
    ap.add_argument("--coq", default="coqc",
                    help="coqc/rocq binary for `Set Printing All` elaboration "
                         "(default: coqc)")
    args = ap.parse_args()

    with open(args.input) as f:
        text = f.read()
    report = Report()
    try:
        elab = rocq_elaborate(text, args.input, args.coq, statement_names(text))
        src = translate_unit(text, report, elab)
    except Untranslatable as e:
        if args.report:
            print(f"FLAG: {e.reason}")
            sys.exit(2)
        sys.stderr.write(f"Untranslatable: {e.reason}\n")
        sys.exit(2)

    if args.report:
        print(f"OK ({args.input})")
        for k in sorted(report.handled):
            print(f"  handled {k}: {report.handled[k]}")
    else:
        sys.stdout.write(src)


if __name__ == "__main__":
    main()
