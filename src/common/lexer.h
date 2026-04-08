#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

#include "src/common/options.h"

// clang-format off
typedef enum {
    TOK_IDENT,      // identifiers

    // keywords
    #define X(lexeme, tok, dbg) tok,
    #include "src/common/token_keywords.def"
    #undef X

    TOK_LPAREN,      // (
    TOK_RPAREN,      // )
    TOK_LBRACE,      // {
    TOK_RBRACE,      // }
    TOK_COLON,       // :
    TOK_COLON_EQ,    // :=
    TOK_COMMA,       // ,
    TOK_DOT,         // .
    TOK_DARROW,      // =>
    TOK_PIPE,        // |
    TOK_LEFT_ARROW,    // <-
    TOK_SEMICOLON,     // ;
    TOK_DOUBLE_PIPE,   // ||
    TOK_LBRACKET,      // [
    TOK_RBRACKET,      // ]

    TOK_COMMENT,
    TOK_EOF,
    TOK_ERROR,

    // magic: we can add a sentinel here and use it as a size
    NUM_TOKEN_TYPES,
} TokenType;
// clang-format on

typedef struct {
    TokenType type;
    char *lexeme;  // malloc'ed string only for TOK_IDENT, otherwise null
                   // pointer. lexer_free_token will free this.
    int pos;       // position in the input where the token starts
} Token;

typedef struct {
    const char *src;          // entire input buffer
    int pos;                  // current char index
    MEngineOptions *options;  // Parser options
} Lexer;

typedef struct {
    const char *name;
    TokenType kind;
} Keyword;

static const Keyword keywords[] = {
#define X(lexeme, tok, dbg) {lexeme, tok},
#include "src/common/token_keywords.def"
#undef X
};

/* Lexer API */

/**
 * Initialize the lexer with the input string.
 *
 * @param lx Pointer to the Lexer to initialize.
 * @param input The input string to lex. Lexer does not assume ownership of this
 * string.
 * @param options Pointer to MEngineOptions for lexer configuration.
 */
void lexer_init(Lexer *lx, const char *input, MEngineOptions *options);

// Get the next token from the lexer
// The returned Token* is malloc'ed. The caller is responsible for
// freeing it with `lexer_free_token`.
Token *lexer_next_token(Lexer *lx);

// Peek at the next token without consuming it
// The returned Token* is malloc'ed. The caller is responsible for
// freeing it with `lexer_free_token`.
Token *lexer_peek_token(Lexer *lx);

// Utility: free a token (frees both lexeme and the Token struct)
void lexer_free_token(Token *t);

#endif  // LEXER_H