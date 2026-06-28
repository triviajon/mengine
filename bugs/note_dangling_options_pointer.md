# Not fixed: dangling `MEngineOptions *` (use-after-return under ASan)

**Status:** pre-existing, **not fixed**.  Discovered while validating the
parametric-list kernel fixes (`stdlib-benchmark` branch) under AddressSanitizer;
unrelated to those fixes.  Documented here so the next person who runs `make
check` under ASan knows it is a known, separate issue rather than a regression.

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

## Fix options (when someone takes this on)

1. **Own the options (preferred):** make `mengine_runtime_new` copy the struct
   (`rt->options = malloc(sizeof *options); *rt->options = *options;`) and free it
   in `mengine_runtime_free`.  Removes the lifetime footgun for every caller.
   Watch for code that *mutates* `rt->options` and expects the caller to observe
   it (e.g. `runtime.c:76-79` toggles `quiet`; that stays internal, so a copy is
   fine) and for callers that share one options struct across runtimes.
2. **Fix the caller only:** give `make_rt`'s options a lifetime ≥ the runtime
   (a `static`/heap allocation).  Narrower; leaves the by-reference API as a trap
   for the next caller.

No `.me` reproducer: this is a C-level lifetime bug in the runtime/test harness,
not a kernel or proof-checking bug, and it cannot affect proof soundness.
