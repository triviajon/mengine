#ifndef PARSER_H
#define PARSER_H

#include "src/common/lexer.h"
#include "src/common/parser_base.h"

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
    AST **branches;  // linked list of "| pattern => body"
    size_t *branch_count;
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
