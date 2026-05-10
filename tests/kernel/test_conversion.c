#include "src/kernel/kernel_api.h"
#include "tests/helpers/test_framework.h"

static void test_beta_conversion_evidence(void) {
    test_start("conversion records beta convertibility");

    Context *ctx = kernel_context_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *x = kernel_var_create("x", A, A);
    Expression *id = kernel_lambda_create(x, x);
    Expression *a = kernel_var_create("a", A, A);
    Expression *app = kernel_app_create(id, a, A);

    Conversion *conv = kernel_expr_conversion_in_context(A, app, a);
    assert_not_null(conv, "beta-redex should convert to its reduct");

    kernel_conversion_free(conv);
}

static void test_conversion_weakens_to_extension(void) {
    test_start("conversion evidence is valid in context extensions");

    Context *ctx = kernel_context_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *x = kernel_var_create("x", A, A);
    Expression *id = kernel_lambda_create(x, x);
    Expression *a = kernel_var_create("a", A, A);
    Expression *app = kernel_app_create(id, a, A);
    Expression *y = kernel_var_create("y", A, A);

    Conversion *conv = kernel_expr_conversion_in_context(A, app, a);
    assert_not_null(conv, "beta conversion should be available");
    assert_true(kernel_conversion_valid_in_context(conv, y),
                "conversion over a smaller context should weaken to an extension");

    Conversion *extended_conv = kernel_expr_conversion_in_context(y, app, a);
    assert_not_null(extended_conv, "conversion should be checkable in an extension context");

    kernel_conversion_free(extended_conv);
    kernel_conversion_free(conv);
}

void run_conversion_tests(void) {
    test_suite_start("kernel/conversion");

    test_beta_conversion_evidence();
    test_conversion_weakens_to_extension();

    test_suite_end();
}
