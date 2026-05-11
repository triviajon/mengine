#include <stdio.h>

#include "src/kernel/kernel_api.h"
#include "tests/helpers/test_framework.h"

static void test_order_deep_chain(void) {
    test_start("order answers ancestor queries in a deep chain");

    Context *empty = kernel_context_empty();
    Expression *type = kernel_type_create();
    Expression *A = kernel_var_create("A", type, empty);

    enum { N = 160 };
    Expression *vars[N];
    vars[0] = A;

    Context *ctx = A;
    for (int i = 1; i < N; i++) {
        char name[32];
        snprintf(name, sizeof(name), "x%d", i);
        vars[i] = kernel_var_create(name, A, ctx);
        ctx = vars[i];
    }

    assert_true(kernel_context_is_ancestor(empty, vars[N - 1]),
                "empty context should be an ancestor of every context");

    for (int i = 0; i < N; i += 17) {
        for (int j = i; j < N; j += 19) {
            assert_true(kernel_context_is_ancestor(vars[i], vars[j]),
                        "earlier chain variable should be an ancestor of later variable");
        }
    }

    assert_false(kernel_context_is_ancestor(vars[N - 1], vars[N / 2]),
                 "later chain variable should not be an ancestor of earlier variable");
}

static void test_order_rejects_sibling_branches(void) {
    test_start("order rejects variables from sibling context branches");

    Context *empty = kernel_context_empty();
    Expression *type = kernel_type_create();
    Expression *A = kernel_var_create("A", type, empty);

    Expression *left = kernel_var_create("left", A, A);
    Expression *left_child = kernel_var_create("left_child", A, left);
    Expression *right = kernel_var_create("right", A, A);
    Expression *right_child = kernel_var_create("right_child", A, right);

    assert_true(kernel_context_is_ancestor(left, left_child),
                "left should be an ancestor of its child");
    assert_true(kernel_context_is_ancestor(right, right_child),
                "right should be an ancestor of its child");
    assert_false(kernel_context_is_ancestor(left, right_child),
                 "left branch should not contain right child");
    assert_false(kernel_context_is_ancestor(right, left_child),
                 "right branch should not contain left child");
}

void run_order_tests(void) {
    test_suite_start("kernel/order");

    test_order_deep_chain();
    test_order_rejects_sibling_branches();

    test_suite_end();
}
