#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

#include "src/common/parser_base.h"

// Forward declaration of the AST.
typedef struct AST AST;

typedef struct {
    char *name;  // identifier name
    AST *type;   // AST node representing the type
} Binder;

typedef struct {
    char *constructor_name;  // Constructor name
    char **argument_names;   // Array of argument names
    int argument_count;      // Number of arguments
} Pattern;

typedef enum {
    AST_VAR,
    AST_TYPE,
    AST_PROP,
    AST_LAMBDA,
    AST_FORALL,
    AST_LET,
    AST_FIX,
    AST_MATCHBRANCH,
    AST_MATCH,
    AST_APP,
    AST_PATVAR,
} ASTTag;

typedef struct {
    char *name;
} VarAST;

typedef struct {
    char *name;
} PatVarAST;

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
    char *name;
    AST *type;
    AST *value;
    AST *body;
} LetAST;

typedef struct {
    char *name;
    Binder **binders;
    size_t binder_count;
    char *decreasing_arg_name;
    AST *return_type;
    AST *body;
} FixAST;

typedef struct {
    Pattern *pattern;
    AST *body;
} MatchBranchAST;

typedef struct {
    AST *scrutinee;
    AST **branches;  // linked list of "| pattern => body"
    size_t branch_count;
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
        PatVarAST patvar;
        LambdaAST lambda;
        ForallAST forall;
        LetAST let;
        AppAST app;
        MatchBranchAST matchbranch;
        MatchAST match;
        FixAST fix;
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
 *                 | <let_expr>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed prefix term.
 */
AST *parse_prefix_term(Parser *p);

/**
 * <lambda_expr>  ::= "fun" "(" <binder> ")" "=>" <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed lambda expression.
 */
AST *parse_lambda(Parser *p);

/**
 * <forall_expr>  ::= "forall" "(" <binder> ")" "," <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed forall expression.
 */
AST *parse_forall(Parser *p);

/**
 * <fix_expr> ::= "fix" <identifier> { "(" <binder> ")" } <decreasing_arg_annotation> ":" <term> ":=" <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed fix expression.
 */
AST *parse_fix(Parser *p);

/**
 * <decreasing_arg_annotation> ::= "{" "struct" <identifier> "}"
 *
 * @param p Pointer to the Parser.
 * @return Name of the decreasing argument.
 */
char *parse_decreasing_arg_annotation(Parser *p);

/**
 * <binder> ::= <ident> ":" <term>
 *
 * @param p Pointer to the Parser.
 * @return Pointer to a Binder structure containing identifier and type.
 */
Binder *parse_binder(Parser *p);

/**
 * <let_expr>     ::= "let" <ident> ":" <term> ":=" <term> "in" <term>
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed let expression.
 */
AST *parse_let(Parser *p);

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

/**
 * Parse a term pattern (like parse_term but also handles ?X pattern variables).
 *
 * @param p Pointer to the Parser.
 * @return AST node representing the parsed term pattern.
 */
AST *parse_term_pattern(Parser *p);

/**
 * Print an AST node to a file stream (for debugging).
 *
 * @param stream The file stream to print to (stdout, stderr, etc).
 * @param ast The AST node to print.
 */
void fprint_ast(FILE *stream, AST *ast);

/**
 * Print an AST node to stdout (for debugging).
 * Convenience wrapper around fprint_ast.
 *
 * @param ast The AST node to print.
 */
void print_ast(AST *ast);

/**
 * Debug print an AST node with formatting similar to lexer token printing.
 * Only prints if debug flags are enabled in the parser's options.
 *
 * @param p Pointer to the Parser (for accessing debug options).
 * @param ast The AST node to print.
 */
void debug_print_ast(Parser *p, AST *ast);

#endif  // PARSER_H
