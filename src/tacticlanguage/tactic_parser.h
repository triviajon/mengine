#ifndef TACTIC_PARSER_H
#define TACTIC_PARSER_H

#include "src/termlanguage/parser.h"

typedef enum {
    TACTIC_ADMITTED,
    TACTIC_INTRO,
    TACTIC_INTROS,
    TACTIC_APPLY,
    TACTIC_EAPPLY,
    TACTIC_EXACT,
    TACTIC_REWRITE,
    TACTIC_REWRITE_BACKWARD,
    TACTIC_EREWRITE,
    TACTIC_EREWRITE_BACKWARD,
    TACTIC_REFLEXIVITY,
    TACTIC_ASSUMPTION,
    TACTIC_SPLIT,
    TACTIC_LEFT,
    TACTIC_RIGHT,
    TACTIC_EXISTS,
    TACTIC_CBV
} TacticTag;

typedef struct {
    char *name;
} IntroTactic;

typedef struct {
    char **names;       // Array of names
    size_t name_count;  // Number of names
} IntrosTactic;

typedef struct {
    AST *lemma;
} ApplyTactic;

typedef struct {
    AST *lemma;
} EapplyTactic;

typedef struct {
    AST *proof_term;
} ExactTactic;

typedef struct {
    AST *lemma;
    AST *equiv_proof;  // Proof of Equivalence A R
    bool backward;     // true if "rewrite <-"
} RewriteTactic;

typedef struct {
    AST *witness;
} ExistsTactic;

typedef struct {
    char **rules;
    int rules_count;
} CbvTactic;

typedef struct Tactic {
    TacticTag tag;
    union {
        IntroTactic intro;
        IntrosTactic intros;
        ApplyTactic apply;
        EapplyTactic eapply;
        ExactTactic exact;
        RewriteTactic rewrite;
        ExistsTactic exists;
        CbvTactic cbv;
    } as;
} Tactic;

char *tactic_tag_to_string(TacticTag tag);
void free_tactic(Tactic *tac);

// Forward declaration - full definition in tactic_ast.h
typedef struct TacticExpr TacticExpr;

/**
 * <proof_command> ::= <tactic_expr> "." | "Admitted" "."
 * <tactic_expr>   ::= <tactic_seq> { "||" <tactic_seq> }
 * <tactic_seq>    ::= <tactic_atom> { ";" <tactic_atom> }
 * <tactic_atom>   ::= <primitive_tactic>
 *                    | "try" <tactic_atom>
 *                    | "repeat" <tactic_atom>
 *                    | "first" "[" <tactic_expr> { "|" <tactic_expr> } "]"
 *                    | "idtac"
 *                    | "fail"
 *                    | "(" <tactic_expr> ")"
 *
 * @param p Pointer to the Parser.
 * @return TacticExpr structure representing the parsed proof command.
 */
TacticExpr *tactic_parse_proof_command(Parser *p);

/**
 * Parse a tactic expression (without consuming trailing '.').
 * Used by the command parser for parsing Tactic definition bodies.
 *
 * @param p Pointer to the Parser.
 * @return TacticExpr structure representing the parsed expression.
 */
TacticExpr *tactic_parse_expr(Parser *p);

#endif  // TACTIC_PARSER_H
