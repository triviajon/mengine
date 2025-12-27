#include "src/common/lexer.h"
#include "src/metalanguage/parser.h"
#include "tests/helpers/test_framework.h"

void test_parse_variable(void) {
    test_start("covers simple variable parsing");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_VAR, ast->tag, "AST should be VAR");
    assert_equal_str("x", ast->value.var.name, "variable name should be 'x'");
}

void test_parse_type(void) {
    test_start("covers Type keyword parsing");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "Type", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_TYPE, ast->tag, "AST should be TYPE");
}

void test_parse_prop(void) {
    test_start("covers Prop keyword parsing");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "Prop", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_PROP, ast->tag, "AST should be PROP");
}

void test_parse_lambda_simple(void) {
    test_start("covers simple lambda expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun (x : Type) => x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LAMBDA, ast->tag, "AST should be LAMBDA");
    assert_equal_str("x", ast->value.lambda.binder.name, "binder name should be 'x'");
    assert_not_null(ast->value.lambda.binder.type, "binder type should exist");
    assert_equal_int(AST_TYPE, ast->value.lambda.binder.type->tag, "binder type should be TYPE");
    assert_not_null(ast->value.lambda.body, "lambda body should exist");
    assert_equal_int(AST_VAR, ast->value.lambda.body->tag, "body should be VAR");
    assert_equal_str("x", ast->value.lambda.body->value.var.name, "body variable should be 'x'");
}

void test_parse_lambda_nested(void) {
    test_start("covers nested lambda expressions");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun (f : Type) => fun (x : Type) => f", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LAMBDA, ast->tag, "outer AST should be LAMBDA");
    assert_equal_str("f", ast->value.lambda.binder.name, "outer binder should be 'f'");
    assert_not_null(ast->value.lambda.body, "outer body should exist");
    assert_equal_int(AST_LAMBDA, ast->value.lambda.body->tag, "inner AST should be LAMBDA");
    assert_equal_str("x", ast->value.lambda.body->value.lambda.binder.name,
                     "inner binder should be 'x'");
}

void test_parse_forall_simple(void) {
    test_start("covers simple forall expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "forall (x : Type) , x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_FORALL, ast->tag, "AST should be FORALL");
    assert_equal_str("x", ast->value.forall.binder.name, "binder name should be 'x'");
    assert_not_null(ast->value.forall.binder.type, "binder type should exist");
    assert_equal_int(AST_TYPE, ast->value.forall.binder.type->tag, "binder type should be TYPE");
    assert_not_null(ast->value.forall.body, "forall body should exist");
    assert_equal_int(AST_VAR, ast->value.forall.body->tag, "body should be VAR");
}

void test_parse_forall_nested(void) {
    test_start("covers nested forall expressions");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "forall (x : Type) , forall (y : Type) , x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_FORALL, ast->tag, "outer AST should be FORALL");
    assert_equal_str("x", ast->value.forall.binder.name, "outer binder should be 'x'");
    assert_not_null(ast->value.forall.body, "outer body should exist");
    assert_equal_int(AST_FORALL, ast->value.forall.body->tag, "inner AST should be FORALL");
    assert_equal_str("y", ast->value.forall.body->value.forall.binder.name,
                     "inner binder should be 'y'");
}

void test_parse_lambda_with_forall_body(void) {
    test_start("covers lambda with forall in body");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun (x : Type) => forall (y : Type) , x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LAMBDA, ast->tag, "outer AST should be LAMBDA");
    assert_not_null(ast->value.lambda.body, "lambda body should exist");
    assert_equal_int(AST_FORALL, ast->value.lambda.body->tag, "body should be FORALL");
}

void test_parse_application_single(void) {
    test_start("covers single application (f x)");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "f x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_APP, ast->tag, "AST should be APP");
    assert_not_null(ast->value.app.func, "function should exist");
    assert_equal_int(AST_VAR, ast->value.app.func->tag, "function should be VAR");
    assert_equal_str("f", ast->value.app.func->value.var.name, "function should be 'f'");
    assert_not_null(ast->value.app.arg, "argument should exist");
    assert_equal_int(AST_VAR, ast->value.app.arg->tag, "argument should be VAR");
    assert_equal_str("x", ast->value.app.arg->value.var.name, "argument should be 'x'");
}

void test_parse_application_multiple(void) {
    test_start("covers multiple applications, tests left-associativity");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "f x y", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_APP, ast->tag, "outer AST should be APP");

    // Structure should be (f x) y, so:
    // ast = APP(APP(f, x), y)
    assert_not_null(ast->value.app.arg, "outer arg should exist");
    assert_equal_int(AST_VAR, ast->value.app.arg->tag, "outer arg should be VAR 'y'");
    assert_equal_str("y", ast->value.app.arg->value.var.name, "outer arg should be 'y'");

    assert_not_null(ast->value.app.func, "outer func should exist");
    assert_equal_int(AST_APP, ast->value.app.func->tag, "outer func should be APP");

    AST *inner_app = ast->value.app.func;
    assert_equal_int(AST_VAR, inner_app->value.app.func->tag, "inner func should be VAR 'f'");
    assert_equal_str("f", inner_app->value.app.func->value.var.name, "inner func should be 'f'");
    assert_equal_int(AST_VAR, inner_app->value.app.arg->tag, "inner arg should be VAR 'x'");
    assert_equal_str("x", inner_app->value.app.arg->value.var.name, "inner arg should be 'x'");
}

void test_parse_parenthesized(void) {
    test_start("covers parenthesized expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "(x)", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_VAR, ast->tag, "AST should be VAR (parens removed)");
    assert_equal_str("x", ast->value.var.name, "variable should be 'x'");
}

void test_parse_nested_parenthesized_application(void) {
    test_start("covers nested parenthesized application");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "(f x) (g y)", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_APP, ast->tag, "outer should be APP");

    // First application: (f x)
    assert_not_null(ast->value.app.func, "func should exist");
    assert_equal_int(AST_APP, ast->value.app.func->tag, "func should be APP (f x)");

    // Second application: (g y)
    assert_not_null(ast->value.app.arg, "arg should exist");
    assert_equal_int(AST_APP, ast->value.app.arg->tag, "arg should be APP (g y)");
}

void test_parse_match_single_branch(void) {
    test_start("covers match expression with single branch, boundary case");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "match x with | a => b end", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_MATCH, ast->tag, "AST should be MATCH");
    assert_not_null(ast->value.match.scrutinee, "scrutinee should exist");
    assert_equal_int(AST_VAR, ast->value.match.scrutinee->tag, "scrutinee should be VAR");
    assert_equal_str("x", ast->value.match.scrutinee->value.var.name, "scrutinee should be 'x'");
    assert_equal_int(1, ast->value.match.branch_count, "should have 1 branch");
    assert_not_null(ast->value.match.branches, "branches should exist");
}

void test_parse_match_multiple_branches(void) {
    test_start("covers match expression with multiple branches");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "match b with | true => false | false => true end", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_MATCH, ast->tag, "AST should be MATCH");
    assert_equal_int(2, ast->value.match.branch_count, "should have 2 branches");
    assert_not_null(ast->value.match.branches, "branches should exist");
}

void test_parse_let_simple(void) {
    test_start("covers simple let expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "let x : Type := Type in x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LET, ast->tag, "AST should be LET");
    assert_equal_str("x", ast->value.let.name, "let name should be 'x'");
    assert_not_null(ast->value.let.type, "let type should exist");
    assert_equal_int(AST_TYPE, ast->value.let.type->tag, "type should be TYPE");
    assert_not_null(ast->value.let.value, "let value should exist");
    assert_equal_int(AST_TYPE, ast->value.let.value->tag, "value should be TYPE");
    assert_not_null(ast->value.let.body, "let body should exist");
    assert_equal_int(AST_VAR, ast->value.let.body->tag, "body should be VAR");
    assert_equal_str("x", ast->value.let.body->value.var.name, "body var should be 'x'");
}

void test_parse_let_with_prop(void) {
    test_start("covers let expression binding Prop");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "let x : Type := Prop in x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LET, ast->tag, "AST should be LET");
    assert_not_null(ast->value.let.value, "let value should exist");
    assert_equal_int(AST_PROP, ast->value.let.value->tag, "value should be PROP");
}

void test_parse_let_with_lambda(void) {
    test_start("covers let expression binding a lambda");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "let f : Type := fun (x : Type) => x in f", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LET, ast->tag, "AST should be LET");
    assert_equal_str("f", ast->value.let.name, "let name should be 'f'");
    assert_not_null(ast->value.let.value, "let value should exist");
    assert_equal_int(AST_LAMBDA, ast->value.let.value->tag, "value should be LAMBDA");
}

void test_parse_complex_nested(void) {
    test_start("covers complex nested lambda and forall");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun (x : Type) => fun (y : Type) => forall (z : Type) , x", &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    assert_not_null(ast, "AST should not be null");
    assert_equal_int(AST_LAMBDA, ast->tag, "outer should be LAMBDA");
    assert_not_null(ast->value.lambda.body, "first body should exist");
    assert_equal_int(AST_LAMBDA, ast->value.lambda.body->tag, "second level should be LAMBDA");
    assert_not_null(ast->value.lambda.body->value.lambda.body, "second body should exist");
    assert_equal_int(AST_FORALL, ast->value.lambda.body->value.lambda.body->tag,
                     "third level should be FORALL");
}

int main() {
    test_suite_start("Parser Test Suite");

    // Atomic expressions (boundaries)
    test_parse_variable();
    test_parse_type();
    test_parse_prop();

    // Lambda expressions
    test_parse_lambda_simple();
    test_parse_lambda_nested();

    // Forall expressions
    test_parse_forall_simple();
    test_parse_forall_nested();

    // Mixed lambda and forall
    test_parse_lambda_with_forall_body();

    // Applications
    test_parse_application_single();
    test_parse_application_multiple();

    // Parenthesized expressions
    test_parse_parenthesized();
    test_parse_nested_parenthesized_application();

    // Match expressions
    test_parse_match_single_branch();
    test_parse_match_multiple_branches();

    // Let expressions
    test_parse_let_simple();
    test_parse_let_with_prop();
    test_parse_let_with_lambda();

    // Complex nested expressions
    test_parse_complex_nested();

    test_suite_end();
    print_test_summary();
    return get_test_failures();
}
