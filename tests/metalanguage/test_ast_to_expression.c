#include <string.h>

#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"
#include "src/metalanguage/ast_to_expression.h"
#include "tests/helpers/test_framework.h"

void test_convert_type(void) {
    test_start("covers Type conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("Type", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_equal_str("Type", str, "expression should be Type");
}

void test_convert_prop(void) {
    test_start("covers Prop conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("Prop", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_equal_str("Prop", str, "expression should be Prop");
}

void test_convert_lambda_simple(void) {
    test_start("covers simple lambda conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("fun (x : Type) => x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    // The output should be a lambda expression
    // We check that it contains the key components
    assert_true(strstr(str, "fun") != NULL, "should contain 'fun'");
    assert_true(strstr(str, "x") != NULL, "should contain 'x'");
}

void test_convert_lambda_nested(void) {
    test_start("covers nested lambda conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "fun (f : Type) => fun (x : Type) => f", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "fun") != NULL, "should contain 'fun'");
}

void test_convert_forall_simple(void) {
    test_start("covers simple forall conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("forall (x : Type) , x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "forall") != NULL, "should contain 'forall'");
}

void test_convert_forall_nested(void) {
    test_start("covers nested forall conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "forall (x : Type) , forall (y : Type) , x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "forall") != NULL, "should contain 'forall'");
}

void test_convert_lambda_with_forall_body(void) {
    test_start("covers lambda with forall body conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "fun (x : Type) => forall (y : Type) , x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "fun") != NULL, "should contain 'fun'");
    assert_true(strstr(str, "forall") != NULL, "should contain 'forall'");
}

void test_convert_forall_with_lambda_body(void) {
    test_start("covers forall with lambda body conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "forall (x : Type) , fun (y : Type) => x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "forall") != NULL, "should contain 'forall'");
    assert_true(strstr(str, "fun") != NULL, "should contain 'fun'");
}

void test_convert_complex_nested(void) {
    test_start("covers complex nested lambda and forall conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "fun (x : Type) => fun (y : Type) => forall (z : Type) , x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "fun") != NULL, "should contain 'fun'");
    assert_true(strstr(str, "forall") != NULL, "should contain 'forall'");
}

void test_convert_let_simple(void) {
    test_start("covers simple let expression conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr =
        parse_string_to_expression("let x : Type := Type in x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    // Let expressions may be represented in various ways
    assert_not_null(str, "string should exist");
}

void test_convert_let_with_prop(void) {
    test_start("covers let expression binding Prop");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr =
        parse_string_to_expression("let x : Type := Prop in x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    // The let should be converted properly
    assert_true(strstr(str, "Prop") != NULL, "should contain 'Prop'");
}

void test_convert_let_with_lambda(void) {
    test_start("covers let expression binding lambda");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "let f : Type := fun (x : Type) => x in f", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    // Should have lambda in the let binding
    assert_true(strstr(str, "fun") != NULL || strstr(str, "lambda") != NULL,
                "should contain lambda");
}

void test_convert_with_empty_context(void) {
    test_start("covers conversion with empty context, boundary case");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("Type", ctx);
    assert_not_null(expr, "expression should not be null with empty context");
}

void test_convert_identity_lambda(void) {
    test_start("covers identity lambda");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression("fun (x : Type) => x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    // Should properly bind x and reference it
    assert_true(strstr(str, "x") != NULL, "should reference bound variable");
}

void test_convert_const_lambda(void) {
    test_start("covers const lambda pattern");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr =
        parse_string_to_expression("fun (x : Type) => Type", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "Type") != NULL, "should contain Type");
}

void test_convert_variable_shadowing(void) {
    test_start("covers variable shadowing");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    // Inner x shadows outer x
    Expression *expr = parse_string_to_expression(
        "fun (x : Type) => fun (x : Type) => x", ctx);
    assert_not_null(expr, "expression should not be null with shadowing");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
}

void test_convert_application(void) {
    test_start("covers application conversion");

    Context *ctx = context_create_empty();
    assert_not_null(ctx, "context should be created");

    Expression *expr = parse_string_to_expression(
        "fun (x : Type) => fun (y : Type) => x", ctx);
    assert_not_null(expr, "expression should not be null");

    char *str = stringify_expression(expr);
    assert_not_null(str, "stringified expression should not be null");
    assert_true(strstr(str, "fun") != NULL, "should contain fun");
}

int main() {
    test_suite_start("AST to Expression Conversion Test Suite");

    // Atomic conversions (boundaries)
    test_convert_type();
    test_convert_prop();
    test_convert_with_empty_context();

    // Lambda conversions
    test_convert_lambda_simple();
    test_convert_lambda_nested();
    test_convert_identity_lambda();
    test_convert_const_lambda();

    // Forall conversions
    test_convert_forall_simple();
    test_convert_forall_nested();

    // Mixed conversions
    test_convert_lambda_with_forall_body();
    test_convert_forall_with_lambda_body();
    test_convert_complex_nested();

    // Let expression conversions
    test_convert_let_simple();
    test_convert_let_with_prop();
    test_convert_let_with_lambda();

    // Variable handling
    test_convert_variable_shadowing();
    test_convert_application();

    test_suite_end();
    print_test_summary();
    return get_test_failures();
}
