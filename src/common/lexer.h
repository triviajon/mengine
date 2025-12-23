#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include <stddef.h>

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
    TOK_LET,     // let
    TOK_IN,      // in
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
    TOK_PRINT,       // Print
    TOK_SHOW,        // Show
    TOK_CONTEXT,     // Context
    TOK_PROOF,       // Proof
    TOK_GOAL,        // Goal
    TOK_STATE,       // State
    TOK_INDUCTIVE,   // Inductive

    // tactic keywords
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

    TOK_COMMENT,
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

typedef struct {
    const char *name;
    TokenType kind;
} Keyword;

// clang-format off
static const Keyword keywords[] = {
    #define KEYWORD(name, tok) {name, tok},
        KEYWORD("fun", TOK_FUN)
        KEYWORD("forall", TOK_FORALL)
        KEYWORD("Type", TOK_TYPE)
        KEYWORD("Prop", TOK_PROP)
        KEYWORD("let", TOK_LET)
        KEYWORD("in", TOK_IN)
        KEYWORD("match", TOK_MATCH)
        KEYWORD("with", TOK_WITH)
        KEYWORD("end", TOK_END)
        KEYWORD("Axiom", TOK_AXIOM)
        KEYWORD("Variable", TOK_VARIABLE)
        KEYWORD("Definition", TOK_DEFINITION)
        KEYWORD("Theorem", TOK_THEOREM)
        KEYWORD("Lemma", TOK_LEMMA)
        KEYWORD("Check", TOK_CHECK)
        KEYWORD("Print", TOK_PRINT)
        KEYWORD("Show", TOK_SHOW)
        KEYWORD("Context", TOK_CONTEXT)
        KEYWORD("Proof", TOK_PROOF)
        KEYWORD("Goal", TOK_GOAL)
        KEYWORD("State", TOK_STATE)
        KEYWORD("Inductive", TOK_INDUCTIVE)
        KEYWORD("Admitted", TOK_ADMITTED)
        KEYWORD("intro", TOK_INTRO)
        KEYWORD("intros", TOK_INTROS)
        KEYWORD("apply", TOK_APPLY)
        KEYWORD("eapply", TOK_EAPPLY)
        KEYWORD("exact", TOK_EXACT)
        KEYWORD("rewrite", TOK_REWRITE)
        KEYWORD("reflexivity", TOK_REFLEXIVITY)
        KEYWORD("assumption", TOK_ASSUMPTION)
        KEYWORD("split", TOK_SPLIT)
        KEYWORD("left", TOK_LEFT)
        KEYWORD("right", TOK_RIGHT)
        KEYWORD("exists", TOK_EXISTS)
};
// clang-format on

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