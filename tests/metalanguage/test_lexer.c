#include "src/common/lexer.h"
#include "tests/helpers/test_framework.h"

static Token *get_next_token(Lexer *lexer) { return lexer_next_token(lexer); }

// Test empty input
void test_empty_input(void) {
    test_start("covers empty input");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_EOF, tok->type, "empty input should return EOF");
    lexer_free_token(tok);
}

// Test single character identifier
void test_single_char_identifier(void) {
    test_start("covers single character identifier, boundary case");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "lexeme should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_EOF, tok->type, "should reach EOF");
    lexer_free_token(tok);
}

// Test multi-character identifier
void test_multi_char_identifier(void) {
    test_start("covers multi-character identifier");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "variable", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("variable", tok->lexeme, "lexeme should be 'variable'");
    lexer_free_token(tok);
}

// Test identifier with underscore prefix
void test_identifier_underscore_prefix(void) {
    test_start("covers identifier starting with underscore");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "_var", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("_var", tok->lexeme, "lexeme should be '_var'");
    lexer_free_token(tok);
}

// Test identifier with digits
void test_identifier_with_digits(void) {
    test_start("covers identifier with digits");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "var123", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("var123", tok->lexeme, "lexeme should be 'var123'");
    lexer_free_token(tok);
}

// Test identifier with underscores and digits
void test_identifier_complex(void) {
    test_start("covers identifier with underscores and digits");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "_var_123_x", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("_var_123_x", tok->lexeme,
                     "lexeme should be '_var_123_x'");
    lexer_free_token(tok);
}

// Test 'fun' keyword (not identifier)
void test_keyword_fun(void) {
    test_start("covers keyword 'fun', boundary between keyword and identifier");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_FUN, tok->type, "should be FUN keyword");
    lexer_free_token(tok);
}

// Test 'function' identifier (not 'fun' keyword)
void test_identifier_function(void) {
    test_start("covers identifier 'function', boundary with keyword 'fun'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "function", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier, not keyword");
    assert_equal_str("function", tok->lexeme, "lexeme should be 'function'");
    lexer_free_token(tok);
}

// Test 'forall' keyword
void test_keyword_forall(void) {
    test_start("covers keyword 'forall'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "forall", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_FORALL, tok->type, "should be FORALL keyword");
    lexer_free_token(tok);
}

// Test 'Type' keyword
void test_keyword_type(void) {
    test_start("covers keyword 'Type'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "Type", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_TYPE, tok->type, "should be TYPE keyword");
    lexer_free_token(tok);
}

// Test 'Prop' keyword
void test_keyword_prop(void) {
    test_start("covers keyword 'Prop'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "Prop", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_PROP, tok->type, "should be PROP keyword");
    lexer_free_token(tok);
}

// Test 'match' keyword
void test_keyword_match(void) {
    test_start("covers keyword 'match'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "match", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_MATCH, tok->type, "should be MATCH keyword");
    lexer_free_token(tok);
}

// Test 'with' keyword
void test_keyword_with(void) {
    test_start("covers keyword 'with'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "with", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_WITH, tok->type, "should be WITH keyword");
    lexer_free_token(tok);
}

// Test 'end' keyword
void test_keyword_end(void) {
    test_start("covers keyword 'end'");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "end", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_END, tok->type, "should be END keyword");
    lexer_free_token(tok);
}

// Test operators
void test_operator_lparen(void) {
    test_start("covers left parenthesis operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "(", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_LPAREN, tok->type, "should be LPAREN");
    lexer_free_token(tok);
}

void test_operator_rparen(void) {
    test_start("covers right parenthesis operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, ")", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_RPAREN, tok->type, "should be RPAREN");
    lexer_free_token(tok);
}

void test_operator_colon(void) {
    test_start("covers colon operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, ":", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_COLON, tok->type, "should be COLON");
    lexer_free_token(tok);
}

void test_operator_comma(void) {
    test_start("covers comma operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, ",", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_COMMA, tok->type, "should be COMMA");
    lexer_free_token(tok);
}

void test_operator_dot(void) {
    test_start("covers dot operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, ".", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_DOT, tok->type, "should be DOT");
    lexer_free_token(tok);
}

void test_operator_darrow(void) {
    test_start("covers double arrow operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "=>", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_DARROW, tok->type, "should be DARROW");
    lexer_free_token(tok);
}

void test_operator_pipe(void) {
    test_start("covers pipe operator");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "|", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_PIPE, tok->type, "should be PIPE");
    lexer_free_token(tok);
}

// Test multiple tokens with no whitespace
void test_multiple_tokens_no_space(void) {
    test_start("covers multiple tokens with no whitespace");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "(x)", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_LPAREN, tok->type, "first should be LPAREN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_RPAREN, tok->type, "third should be RPAREN");
    lexer_free_token(tok);
}

// Test tokens with single space
void test_tokens_single_space(void) {
    test_start("covers tokens separated by single space");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x y", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "first should be identifier");
    assert_equal_str("x", tok->lexeme, "first identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("y", tok->lexeme, "second identifier should be 'y'");
    lexer_free_token(tok);
}

// Test tokens with multiple spaces
void test_tokens_multiple_spaces(void) {
    test_start("covers tokens separated by multiple spaces");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x   y", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "first should be identifier");
    assert_equal_str("x", tok->lexeme, "first identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("y", tok->lexeme, "second identifier should be 'y'");
    lexer_free_token(tok);
}

// Test tokens with tabs
void test_tokens_with_tabs(void) {
    test_start("covers tokens separated by tabs");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x\ty", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "first should be identifier");
    assert_equal_str("x", tok->lexeme, "first identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("y", tok->lexeme, "second identifier should be 'y'");
    lexer_free_token(tok);
}

// Test tokens with newlines
void test_tokens_with_newlines(void) {
    test_start("covers tokens separated by newlines");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x\ny", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "first should be identifier");
    assert_equal_str("x", tok->lexeme, "first identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("y", tok->lexeme, "second identifier should be 'y'");
    lexer_free_token(tok);
}

// Test tokens with mixed whitespace
void test_tokens_mixed_whitespace(void) {
    test_start("covers tokens separated by mixed whitespace");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "x  \t\n  y", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "first should be identifier");
    assert_equal_str("x", tok->lexeme, "first identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "second should be identifier");
    assert_equal_str("y", tok->lexeme, "second identifier should be 'y'");
    lexer_free_token(tok);
}

// Test lambda expression (realistic case)
void test_lambda_expression(void) {
    test_start("covers complete lambda expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "fun (x : Type) => x", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_FUN, tok->type, "should be FUN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_LPAREN, tok->type, "should be LPAREN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_COLON, tok->type, "should be COLON");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_TYPE, tok->type, "should be TYPE");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_RPAREN, tok->type, "should be RPAREN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_DARROW, tok->type, "should be DARROW");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_EOF, tok->type, "should reach EOF");
    lexer_free_token(tok);
}

// Test forall expression
void test_forall_expression(void) {
    test_start("covers complete forall expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "forall (x : Type) , x", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_FORALL, tok->type, "should be FORALL");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_LPAREN, tok->type, "should be LPAREN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_COLON, tok->type, "should be COLON");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_TYPE, tok->type, "should be TYPE");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_RPAREN, tok->type, "should be RPAREN");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_COMMA, tok->type, "should be COMMA");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_EOF, tok->type, "should reach EOF");
    lexer_free_token(tok);
}

// Test match expression
void test_match_expression(void) {
    test_start("covers complete match expression");

    MEngineOptions options = {.debug = false};
    Lexer lexer;
    lexer_init(&lexer, "match x with | a => b end", &options);

    Token *tok = get_next_token(&lexer);
    assert_equal_int(TOK_MATCH, tok->type, "should be MATCH");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("x", tok->lexeme, "identifier should be 'x'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_WITH, tok->type, "should be WITH");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_PIPE, tok->type, "should be PIPE");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("a", tok->lexeme, "identifier should be 'a'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_DARROW, tok->type, "should be DARROW");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_IDENT, tok->type, "should be identifier");
    assert_equal_str("b", tok->lexeme, "identifier should be 'b'");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_END, tok->type, "should be END");
    lexer_free_token(tok);

    tok = get_next_token(&lexer);
    assert_equal_int(TOK_EOF, tok->type, "should reach EOF");
    lexer_free_token(tok);
}

int main() {
    test_suite_start("Lexer Test Suite");

    // Boundaries and partitions
    test_empty_input();
    test_single_char_identifier();

    // Identifiers
    test_multi_char_identifier();
    test_identifier_underscore_prefix();
    test_identifier_with_digits();
    test_identifier_complex();

    // Keywords vs identifiers (boundaries)
    test_keyword_fun();
    test_identifier_function();
    test_keyword_forall();
    test_keyword_type();
    test_keyword_prop();
    test_keyword_match();
    test_keyword_with();
    test_keyword_end();

    // Operators
    test_operator_lparen();
    test_operator_rparen();
    test_operator_colon();
    test_operator_comma();
    test_operator_dot();
    test_operator_darrow();
    test_operator_pipe();

    // Multiple tokens
    test_multiple_tokens_no_space();

    // Whitespace variations
    test_tokens_single_space();
    test_tokens_multiple_spaces();
    test_tokens_with_tabs();
    test_tokens_with_newlines();
    test_tokens_mixed_whitespace();

    // Realistic expressions
    test_lambda_expression();
    test_forall_expression();
    test_match_expression();

    test_suite_end();
    print_test_summary();
    return get_test_failures();
}
