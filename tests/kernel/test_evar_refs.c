#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/kernel_api.h"
#include "tests/helpers/test_framework.h"

// Filling a hole with an evar-free term succeeds without walking the term.
void test_fill_hole_evar_free_term(void) {
    test_start("fill_hole succeeds with evar-free term");

    Context *ctx = context_create_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *hole = kernel_hole_create("goal", A, A);
    Expression *a = kernel_var_create("a", A, A);

    assert_false(has_holes(a), "a should be evar-free");

    bool result = kernel_hole_fill(hole, a);
    assert_true(result, "filling hole with evar-free term should succeed");
}

// Cyclic instantiation (?goal := fun x => ?goal x) must be rejected.
void test_fill_hole_cyclic_rejected(void) {
    test_start("fill_hole rejects cyclic instantiation");

    Context *ctx = context_create_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *arrow_A_A = kernel_arrow_create(A, A, A);
    Expression *hole = kernel_hole_create("goal", arrow_A_A, A);

    assert_true(has_holes(hole), "hole should have holes (itself)");

    Expression *x = kernel_var_create("x", A, A);
    Expression *app = kernel_app_create(hole, x, x);
    Expression *lam = kernel_lambda_create(x, app);

    assert_true(has_holes(lam), "lambda containing hole should have holes");

    bool result = kernel_hole_fill(hole, lam);
    assert_false(result, "filling hole with term containing itself should fail");
}

// evar_refs propagates correctly through term constructors.
void test_evar_refs_propagation(void) {
    test_start("evar_refs propagates through term construction");

    Context *ctx = context_create_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *hole = kernel_hole_create("h", A, A);
    Expression *arrow_A_A = kernel_arrow_create(A, A, A);
    Expression *id = kernel_var_create("id", arrow_A_A, A);
    Expression *app = kernel_app_create(id, hole, A);

    assert_true(has_holes(hole), "hole should have holes");
    assert_true(has_holes(app), "app containing hole should have holes");
    assert_false(has_holes(id), "id (a variable) should not have holes");
}

// evar tracking includes types, not just term child fields.
void test_evar_refs_propagate_from_types(void) {
    test_start("evar_refs propagate from expression types");

    Context *ctx = context_create_empty();
    Expression *type_hole = kernel_hole_create("A", kernel_type_create(), ctx);
    Expression *x = kernel_var_create("x", type_hole, ctx);

    assert_true(has_holes(x), "variable with hole type should have holes");
}

// After filling a hole, ancestor expressions become evar-free.
void test_fill_hole_clears_evar_refs(void) {
    test_start("fill_hole clears evar_refs from parent expressions");

    Context *ctx = context_create_empty();
    Expression *A = kernel_var_create("A", kernel_type_create(), ctx);
    Expression *hole = kernel_hole_create("h", A, A);
    Expression *x = kernel_var_create("x", A, A);
    Expression *lam = kernel_lambda_create(x, hole);

    assert_true(has_holes(lam), "lambda with hole should have holes");

    bool result = kernel_hole_fill(hole, x);
    assert_true(result, "filling hole with x should succeed");
    assert_false(has_holes(lam), "lambda should be evar-free after filling hole");
}

void run_evar_refs_tests(void) {
    test_suite_start("kernel/evar_refs");

    test_fill_hole_evar_free_term();
    test_fill_hole_cyclic_rejected();
    test_evar_refs_propagation();
    test_evar_refs_propagate_from_types();
    test_fill_hole_clears_evar_refs();

    test_suite_end();
}
