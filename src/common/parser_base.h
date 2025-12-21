#ifndef PARSER_BASE_H
#define PARSER_BASE_H

#include <setjmp.h>
#include <stdbool.h>

#include "src/common/lexer.h"
#include "src/common/options.h"

typedef struct {
    Lexer *lx;                // Underlying lexer
    Token *current;           // Current lookahead token (owned by parser)
    const char *source;       // Source string being parsed
    MEngineOptions *options;  // Parser options
    jmp_buf error_jmp;        // Jump buffer for error recovery
    bool error_recovery_set;  // Whether error recovery is active
} Parser;

/**
 * Initialize the parser with a given lexer.
 *
 * @param p Pointer to the Parser to initialize.
 * @param lx Pointer to the Lexer to use as input.
 * @param options Pointer to MEngineOptions for parser configuration.
 */
void parser_init(Parser *p, Lexer *lx, MEngineOptions *options);

/**
 * Returns true if the parser has no more tokens to consume.
 * @param p Pointer to the Parser to initialize.
 * @return true if the current token is TOK_EOF or NULL, false otherwise.
 */
bool parser_eof(Parser *p);

/**
 * Return the next token and advance the parser.
 *
 * @param p Pointer to the Parser.
 * @return The last Token, for freeing. Advances the parser's current token.
 */
Token *parser_next(Parser *p);

/**
 * Return the current token without consuming it.
 *
 * @param p Pointer to the Parser.
 * @return The current Token.
 */
Token *parser_peek(Parser *p);

/**
 * If the current token matches the given type, consume it and return true.
 * Otherwise, return false without consuming it. This function also frees the
 * consumed token if matched.
 *
 * @param p Pointer to the Parser.
 * @param type The TokenType to match against.
 * @return true if the current token matches the type and was consumed, false
 * otherwise.
 */
bool parser_expect_consume(Parser *p, TokenType type);

/**
 * If the current token matches the given type, return true without consuming
 * it.
 *
 * @param p Pointer to the Parser.
 * @param type The TokenType to match against.
 * @return true if the current token matches the type, false otherwise.
 */
bool parser_expect_no_consume(Parser *p, TokenType type);

/**
 * Report a parse error with the given message.
 *
 * @param p Pointer to the Parser.
 * @param msg The error message to report.
 */
void parser_error(Parser *p, const char *msg);

#endif  // PARSER_BASE_H