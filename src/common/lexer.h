#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

#include "src/common/color.h"
#include "src/common/options.h"

typedef enum {
    // identifiers, punctuation
    TOK_IDENT,
    TOK_LPAREN,    // (
    TOK_RPAREN,    // )
    TOK_COLON,     // :
    TOK_COLON_EQ,  // :=
    TOK_COMMA,     // ,
    TOK_DOT,       // .

    // term keywords
    TOK_DARROW,  // =>
    TOK_FUN,     // fun
    TOK_FORALL,  // forall
    TOK_TYPE,    // Type
    TOK_PROP,    // Prop
    TOK_MATCH,   // match
    TOK_WITH,    // with
    TOK_PIPE,    // |
    TOK_END,     // end

    // command keywords
    TOK_AXIOM,       // Axiom
    TOK_VARIABLE,    // Variable
    TOK_DEFINITION,  // Definition
    TOK_THEOREM,     // Theorem
    TOK_LEMMA,       // Lemma
    TOK_CHECK,       // Check
    TOK_INDUCTIVE,   // Inductive

    // tactic keywords
    TOK_PROOF,        // Proof
    TOK_QED,          // Qed
    TOK_ADMITTED,     // Admitted
    TOK_INTRO,        // intro
    TOK_INTROS,       // intros
    TOK_APPLY,        // apply
    TOK_EAPPLY,       // eapply
    TOK_EXACT,        // exact
    TOK_REWRITE,      // rewrite
    TOK_LEFT_ARROW,   // <-
    TOK_REFLEXIVITY,  // reflexivity
    TOK_ASSUMPTION,   // assumption
    TOK_SPLIT,        // split
    TOK_LEFT,         // left
    TOK_RIGHT,        // right
    TOK_EXISTS,       // exists

    TOK_EOF,
    TOK_ERROR,
} TokenType;

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