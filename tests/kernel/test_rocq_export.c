#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/kernel/kernel_api.h"
#include "tests/helpers/test_framework.h"

static int count_occurrences(const char *haystack, const char *needle) {
    int count = 0;
    for (const char *at = strstr(haystack, needle); at; at = strstr(at + 1, needle)) {
        count++;
    }
    return count;
}

// A subterm shared in the DAG is exported once, as a let named by its address.
void test_rocq_export_shares_subterms(void) {
    test_start("rocq export binds a shared subterm once");

    Context *ctx = kernel_context_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *a = kernel_var_create("a", A, A);
    Expression *g =
        kernel_var_create("g", kernel_arrow_create(A, kernel_arrow_create(A, A, a), a), a);
    Expression *shared = kernel_app_create(kernel_app_create(g, a, g), a, g);
    Expression *body = kernel_app_create(kernel_app_create(g, shared, g), shared, g);
    Expression *d = kernel_var_create_with_body("d", body, g);

    char *export = kernel_rocq_export(d);
    assert_not_null(export, "export should succeed");
    if (!export) {
        return;
    }

    char g_name[64];
    snprintf(g_name, sizeof(g_name), "Axiom v%p :", (void *)g);
    char shared_let[64];
    snprintf(shared_let, sizeof(shared_let), "let s%p :=", (void *)shared);

    assert_true(strncmp(export, "Section MEngineExport.\n", 23) == 0, "export opens a Section");
    assert_true(strstr(export, "End MEngineExport.\n") != NULL, "export closes the Section");
    assert_true(strstr(export, g_name) != NULL, "context entries are named by address");
    assert_equal_int(1, count_occurrences(export, shared_let), "shared subterm is let-bound once");
    free(export);
}

// A definition whose body still contains a hole cannot be exported.
void test_rocq_export_rejects_holes(void) {
    test_start("rocq export rejects a body containing a hole");

    Context *ctx = kernel_context_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *hole = kernel_hole_create("h", A, A);
    Expression *d = kernel_var_create_with_body("d", hole, A);

    assert_null(kernel_rocq_export(d), "export should fail");
}

void run_rocq_export_tests(void) {
    test_suite_start("kernel/rocq_export");

    test_rocq_export_shares_subterms();
    test_rocq_export_rejects_holes();

    test_suite_end();
}
