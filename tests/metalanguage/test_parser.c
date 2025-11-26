#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/lexer.h"
#include "src/metalanguage/parser.h"

void test_parsing(const char *input, const char *description) {
    printf("Test: %s\n", description);
    printf("Input: \"%s\"\n", input);
    printf("AST: ");

    MEngineOptions options = {.debug = false};

    Lexer lexer;
    lexer_init(&lexer, input, &options);

    Parser parser;
    parser_init(&parser, &lexer, &options);

    AST *ast = parse_term(&parser);
    print_ast(ast);
    printf("\n\n");
}

int main() {
    printf("=== Parser Test Suite ===\n\n");

    test_parsing("x", "Simple identifier");
    test_parsing("fun (x : Type) => x", "Lambda expression");
    test_parsing("forall (x : Type) , x", "Forall expression");
    test_parsing("f x y z", "Function application");
    test_parsing("(f x) (g y)", "Nested applications");
    test_parsing("Type Prop", "Type and Prop keywords");
    test_parsing("x   y\n\tz", "Multiple whitespace and newlines");
    test_parsing("match b with | true => false | false => true end",
                 "Match expression with pipes and double arrow");
    test_parsing("match x with | a => b | c => d end",
                 "Pattern match with identifiers");
    test_parsing("fun (n : Type) => forall (x : Type) , f x",
                 "Complex nested expression");

    printf("All tests completed!\n");
    return 0;
}