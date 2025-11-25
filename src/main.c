#include <stdio.h>

#include "src/runtime/runtime.h"

int main(void) {
    MEngineRuntime *rt = mengine_runtime_new();
    if (!rt) {
        fprintf(stderr, "Failed to initialize MEngine runtime.\n");
        return 1;
    }

    mengine_repl(rt);

    mengine_runtime_free(rt);
    return 0;
}
