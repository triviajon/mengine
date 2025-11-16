#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TOK_IDENT,
    TOK_LPAREN,  // (
    TOK_RPAREN,  // )
    TOK_COLON,   // :
    TOK_COMMA,   // ,
    TOK_DOT,     // .
    TOK_ARROW,   // -> (function type / logical implication)
    TOK_DARROW,  // => (pattern match branch)
    TOK_LAMBDA,  // fun
    TOK_FORALL,  // forall
    TOK_TYPE,    // Type
    TOK_PROP,    // Prop
    TOK_MATCH,   // match
    TOK_WITH,    // with
    TOK_PIPE,    // | (pattern separator)
    TOK_END,     // end (end of match)
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;  // malloc'ed string only for TOK_IDENT, otherwise null
                   // pointer. lexer_free_token will free this.
    int pos;       // position in the input where the token starts
} Token;

typedef struct {
    const char *src;  // entire input buffer
    int pos;          // current char index
} Lexer;

/* Lexer API */

/**
 * Initialize the lexer with the input string.
 *
 * @param lx Pointer to the Lexer to initialize. Lexer does not assume ownership
 * of this pointer.
 * @param input The input string to lex. Lexer does not assume ownership of this
 * string.
 */
void lexer_init(Lexer *lx, const char *input);

/**
 * Get the next token from the lexer.
 *
 * @param lx Pointer to the Lexer.
 * @return The next Token. Modifies the lexer's `pos` to point to the beginning
 * of the next token.
 */
Token lexer_next(Lexer *lx);

/**
 * Peek at the next token from the lexer without consuming it.
 *
 * @param lx Pointer to the Lexer.
 * @return The next Token.
 */
Token lexer_peek(Lexer *lx);

/**
 * Free the memory allocated for a token.
 *
 * @param t Pointer to the Token to free.
 */
void lexer_free_token(Token *t);

#endif  // LEXER_H