#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/lexer.h"
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/utils.h"
#include "src/metalanguage/ast_to_expression.h"
#include "src/metalanguage/parser.h"

void print_expression(Expression *expr) {
    printf("%s", stringify_expression(expr));
}

void test_parse_and_convert(const char *input, const char *description) {
    printf("Test: %s\n", description);
    printf("Input: \"%s\"\n", input);
    printf("Expression: ");

    Context *context = context_create_empty();
    if (!context) {
        printf("Failed to create context\n\n");
        return;
    }

    Expression *expr = parse_string_to_expression(input, context);
    if (expr) {
        print_expression(expr);
        printf("\n");
    } else {
        printf("Failed to parse/convert\n");
    }
    printf("\n");
}

void test_with_context(const char *input, const char *description,
                       Context *context) {
    printf("Test: %s\n", description);
    printf("Input: \"%s\"\n", input);
    printf("Expression: ");

    Expression *expr = parse_string_to_expression(input, context);
    if (expr) {
        print_expression(expr);
        printf("\n");
    } else {
        printf("Failed to parse/convert\n");
    }
    printf("\n");
}

int main() {
    printf("=== AST to Expression Test Suite ===\n\n");

    test_parse_and_convert("Type", "Simple Type");
    test_parse_and_convert("Prop", "Simple Prop");
    test_parse_and_convert("fun (x : Type) => x", "Simple lambda");
    test_parse_and_convert("fun (f : Type) => fun (x : Type) => f",
                           "Nested lambdas");
    test_parse_and_convert("forall (x : Type) , x", "Simple forall");
    test_parse_and_convert("forall (x : Type) , forall (y : Type) , x",
                           "Nested foralls");
    test_parse_and_convert("fun (x : Type) => forall (y : Type) , x",
                           "Lambda with forall body");
    test_parse_and_convert("forall (x : Type) , fun (y : Type) => x",
                           "Forall with lambda body");
    test_parse_and_convert(
        "fun (x : Type) => fun (y : Type) => forall (z : Type) , x",
        "Nested lambdas with forall");

    printf("All tests completed!\n");
    return 0;
}
