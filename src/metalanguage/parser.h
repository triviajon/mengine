#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef struct {
    Lexer *lx;       // Underlying lexer
    Token *current;  // Current lookahead token (owned by parser)
} Parser;

/**
 * Initialize the parser with a given lexer.
 *
 * @param p Pointer to the Parser to initialize.
 * @param lx Pointer to the Lexer to use as input.
 */
void parser_init(Parser *p, Lexer *lx);

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

// Forward declaration of the AST.
typedef struct AST AST;

typedef struct {
    char *name;  // identifier name (malloc'ed)
    AST *type;   // AST node representing the type
} Binder;

typedef struct {
    char *name;  // identifier name (malloc'ed)
} Pattern;

typedef enum {
    AST_VAR,
    AST_TYPE,
    AST_PROP,
    AST_LAMBDA,
    AST_FORALL,
    AST_MATCHBRANCH,
    AST_MATCH,
    AST_APP
} ASTTag;

typedef struct {
    char *name;
} VarAST;

typedef struct {
    char *name;
} HoleAST;

typedef struct {
    Binder binder;
    AST *body;
} LambdaAST;

typedef struct {
    Binder binder;
    AST *body;
} ForallAST;

typedef struct {
    Pattern *pattern;
    AST *body;
} MatchBranchAST;

typedef struct {
    AST *scrutinee;
    MatchBranchAST **branches;  // linked list of "| pattern => body"
} MatchAST;

typedef struct {
    AST *func;
    AST *arg;
} AppAST;

struct AST {
    ASTTag tag;
    union {
        VarAST var;
        HoleAST hole;
        LambdaAST lambda;
        ForallAST forall;
        AppAST app;
        MatchBranchAST matchbranch;
        MatchAST match;
    } value;
};

// -------------------------------------------------------------
// Parsing functions (matches BNF grammar)
// -------------------------------------------------------------

/**
 * <term>         ::= <prefix_term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed term.
 */
AST *parse_term(Parser *p);

/**
 * <prefix_term> ::= <lambda_expr>
 *                 | <forall_expr>
 *                 | <match_expr>
 *                 | <application>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed prefix term.
 */
AST *parse_prefix_term(Parser *p);

/**
 * <lambda_expr> ::= ("fun") <binder> "=>" <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed lambda expression.
 */
AST *parse_lambda(Parser *p);

/**
 * <forall_expr> ::= ("forall") <binder> "," <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed forall expression.
 */
AST *parse_forall(Parser *p);

/**
 * <binder> ::= <ident> ":" <term>
 *
 * @param p Pointer to the Parser.
 * @return Binder structure containing identifier and type.
 */
Binder parse_binder(Parser *p);

/**
 * <match_expr>   ::= "match" <term> "with" { <match_branch> } "end"
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed match expression.
 */
AST *parse_match(Parser *p);

/**
 * <match_branch> ::= "|" <pattern> "=>" <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed match branch.
 */
AST *parse_match_branch(Parser *p);

/**
 * <pattern> ::= <ident>
 *
 * @param p Pointer to the Parser.
 * @return Pattern structure representing the parsed pattern.
 */
Pattern *parse_pattern(Parser *p);

/**
 * <application> ::= <atomic> { <atomic> }
 * Left-associative: f x y parses as ((f x) y).
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed application.
 */
AST *parse_application(Parser *p);

/**
 * <atomic> ::= <ident>
 *            | "Type"
 *            | "Prop"
 *            | "(" <term> ")"
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed atomic term.
 */
AST *parse_atomic(Parser *p);

#endif  // PARSER_H
