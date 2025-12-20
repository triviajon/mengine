#include <stdio.h>

#include "src/runtime/repl.h"

int main(void) {
    MEngineOptions options = {.debug = false,
                              .debug__print_tokens = true,
                              .debug__print_ast = true,
                              .debug__print_mode = true};

    MEngineRuntime *rt = mengine_runtime_new(&options);
    if (!rt) {
        fprintf(stderr, "Failed to initialize MEngine runtime.\n");
        return 1;
    }

    mengine_repl(rt);

    mengine_runtime_free(rt);
    return 0;
}
