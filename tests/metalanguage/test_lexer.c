#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "metalanguage/lexer.h"

// Helper to convert token type to string
static const char *token_type_name(TokenType type) {
    switch (type) {
        case TOK_IDENT:
            return "IDENT";
        case TOK_LPAREN:
            return "LPAREN";
        case TOK_RPAREN:
            return "RPAREN";
        case TOK_COLON:
            return "COLON";
        case TOK_COMMA:
            return "COMMA";
        case TOK_DOT:
            return "DOT";
        case TOK_ARROW:
            return "ARROW";
        case TOK_DARROW:
            return "DARROW";
        case TOK_LAMBDA:
            return "LAMBDA";
        case TOK_FORALL:
            return "FORALL";
        case TOK_TYPE:
            return "TYPE";
        case TOK_PROP:
            return "PROP";
        case TOK_MATCH:
            return "MATCH";
        case TOK_WITH:
            return "WITH";
        case TOK_PIPE:
            return "PIPE";
        case TOK_END:
            return "END";
        case TOK_EOF:
            return "EOF";
        case TOK_ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

void test_tokenization(const char *input, const char *description) {
    printf("Test: %s\n", description);
    printf("Input: \"%s\"\n", input);
    printf("Tokens: ");

    Lexer lexer;
    lexer_init(&lexer, input);

    Token tok;
    while (true) {
        tok = lexer_next(&lexer);

        printf("[%s", token_type_name(tok.type));
        if (tok.lexeme != NULL) {
            printf(":%s", tok.lexeme);
        }
        printf("]");

        lexer_free_token(&tok);

        if (tok.type == TOK_EOF || tok.type == TOK_ERROR) {
            break;
        }
        printf(" ");
    }
    printf("\n\n");
}

int main() {
    printf("=== Lexer Test Suite ===\n\n");

    test_tokenization("x", "Simple identifier");
    test_tokenization("fun x : Type => x", "Lambda expression");
    test_tokenization("forall x : Type , x", "Forall expression");
    test_tokenization("f x y z", "Function application");
    test_tokenization("(f x) (g y)", "Nested applications");
    test_tokenization("Type -> Type", "Arrow type");
    test_tokenization("fun x : Type -> Type => x", "Lambda with arrow type");
    test_tokenization("Type Prop", "Type and Prop keywords");
    test_tokenization("(x : Type , y)", "Parentheses and punctuation");
    test_tokenization("x   y\n\tz", "Multiple whitespace and newlines");
    test_tokenization("", "Empty input");
    test_tokenization("match b with | true => false | false => true end",
                      "Match expression with pipes and double arrow");
    test_tokenization("match x with | a => b | c => d end",
                      "Pattern match with identifiers");
    test_tokenization("fun n : Type => forall x : Type , (f x) -> (g x)",
                      "Complex nested expression");
    test_tokenization("_var var1 _123",
                      "Identifiers with underscores and digits");

    printf("All tests completed!\n");
    return 0;
}
