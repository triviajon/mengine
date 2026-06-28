# Fixed: dangling `MEngineOptions *` (use-after-return under ASan)

**Status:** **fixed** — `mengine_runtime_new` now owns a private copy of the
options (`src/runtime/runtime.c`).  Discovered while validating the
parametric-list kernel fixes under AddressSanitizer; pre-existing and unrelated to
those fixes.  Kept as a note because the symptom is subtle (benign without ASan)
and the by-reference API was a footgun.

## Symptom

`make check` built with AddressSanitizer aborts partway through the integration
suite:

```
==…==ERROR: AddressSanitizer: stack-use-after-return on address 0x…
    #0 debug_print_token        src/common/lexer.c:48
    #1 lexer_next_token         src/common/lexer.c:254
    #2 parser_init              src/common/parser_base.c:12
    #3 mengine_runtime_exec_string  src/runtime/runtime.c:127
    #4 run_ok                   tests/engine/test_integration.c:14
    #5 test_axiom_and_check     tests/engine/test_integration.c:30
  …
  #0 …                          tests/engine/test_integration.c:5   (the freed frame)
```

Reproduce (no clang needed — gcc has ASan too):

```bash
make clean
make CC=gcc CFLAGS="-Wall -Wextra -O0 -g -I. -fsanitize=address -fno-omit-frame-pointer" \
         LDFLAGS="-fsanitize=address" check
```

The **default (non-ASan) build is unaffected** — all 431 tests pass — because the
freed stack slot still happens to hold the old option bytes, so the reads return
plausible values.  It is a genuine use-after-return, just a normally-benign one.

## Root cause

`mengine_runtime_new` **borrows** its options by pointer rather than copying them:

```c
// src/runtime/runtime.c
MEngineRuntime *mengine_runtime_new(MEngineOptions *options) {
    …
    rt->options = options;   // stores the caller's pointer, no copy
    …
}
```

The runtime then keeps reading through that pointer for the rest of its life
(`runtime.c:124` `lexer_init(&lx, source, rt->options)` → `lexer_next_token` →
`debug_print_token`, which dereferences `lx->options->debug` at `lexer.c:48`).

The integration-test helper hands it a pointer to a **stack local** and then
returns:

```c
// tests/engine/test_integration.c
static MEngineRuntime *make_rt(void) {
    MEngineOptions opts = {0};        // lives only in this frame
    opts.quiet = true;
    return mengine_runtime_new(&opts);  // runtime keeps &opts after make_rt returns
}
```

When `make_rt` returns, `opts` is gone, but `rt->options` still points at that
reclaimed frame.  The next lex/parse dereferences it → stack-use-after-return.

`src/main.c` avoids the trap only by luck of lifetime: its options struct lives in
`main`'s frame, which outlives the runtime, so the borrowed pointer never dangles.

## The fix

Option 1 (own the options) was taken: `mengine_runtime_new` allocates
`rt->options = malloc(sizeof(MEngineOptions))` and copies the caller's struct into
it (`*rt->options = *options`), and `mengine_runtime_free` frees it.  This removes
the lifetime footgun for every caller — the runtime no longer depends on the
caller keeping the options struct alive.  The transient mutations through
`rt->options` (the `quiet` toggle while loading the prelude, `execution_type`)
were already internal to the runtime, so copying does not change observable
behaviour; `main.c` passes options by value and never reads them back.

The alternative (give the test's options a lifetime ≥ the runtime) was rejected as
narrower — it would have left the by-reference API as a trap for the next caller.

Verified: `make check` built with AddressSanitizer now runs to completion (431/431,
no `stack-use-after-return`).  No `.me` reproducer — this is a C-level lifetime bug
in the runtime, not a kernel or proof-checking bug, and it cannot affect proof
soundness.
