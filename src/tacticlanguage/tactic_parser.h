#ifndef TACTIC_PARSER_H
#define TACTIC_PARSER_H

#include "src/metalanguage/parser.h"

typedef enum {
    TACTIC_ADMITTED,
    TACTIC_INTRO,
    TACTIC_INTROS,
    TACTIC_APPLY,
    TACTIC_EAPPLY,
    TACTIC_EXACT,
    TACTIC_REWRITE,
    TACTIC_REWRITE_BACKWARD,
    TACTIC_REFLEXIVITY,
    TACTIC_ASSUMPTION,
    TACTIC_SPLIT,
    TACTIC_LEFT,
    TACTIC_RIGHT,
    TACTIC_EXISTS
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
    } as;
} Tactic;

char *tactic_tag_to_string(TacticTag tag);

/**
 * <proof_command> ::= <tactic> "." | "Admitted" "."
 *
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the parsed proof command.
 */
Tactic *tactic_parse_proof_command(Parser *p);

/**
 * <tactic> ::= "intro" [ <ident> ]
 *            | "intros" { <ident> }
 *            | "apply" <term>
 *            | "eapply" <term>
 *            | "exact" <term>
 *            | "rewrite" <term> "with" <term>
 *            | "rewrite" "<-" <term> "with" <term>
 *            | "reflexivity"
 *            | "assumption"
 *            | "split"
 *            | "left"
 *            | "right"
 *            | "exists" <term>
 *
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the parsed tactic.
 */
Tactic *tactic_parse_tactic(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the Admitted command.
 */
Tactic *tactic_parse_admitted(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the intro tactic.
 */
Tactic *tactic_parse_intro(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the intros tactic.
 */
Tactic *tactic_parse_intros(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the apply tactic.
 */
Tactic *tactic_parse_apply(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the eapply tactic.
 */
Tactic *tactic_parse_eapply(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the exact tactic.
 */
Tactic *tactic_parse_exact(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the rewrite tactic.
 */
Tactic *tactic_parse_rewrite(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the reflexivity tactic.
 */
Tactic *tactic_parse_reflexivity(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the assumption tactic.
 */
Tactic *tactic_parse_assumption(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the split tactic.
 */
Tactic *tactic_parse_split(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the left tactic.
 */
Tactic *tactic_parse_left(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the right tactic.
 */
Tactic *tactic_parse_right(Parser *p);

/**
 * @param p Pointer to the Parser.
 * @return Tactic structure representing the exists tactic.
 */
Tactic *tactic_parse_exists(Parser *p);

typedef Tactic *(*TacticParseFunc)(Parser *p);

typedef struct {
    TokenType type;
    TacticParseFunc tactic_parse_func;
} TacticDispatchEntry;

static TacticDispatchEntry tactic_dispatch_table[] = {
    {TOK_ADMITTED, tactic_parse_admitted},
    {TOK_INTRO, tactic_parse_intro},
    {TOK_INTROS, tactic_parse_intros},
    {TOK_APPLY, tactic_parse_apply},
    {TOK_EAPPLY, tactic_parse_eapply},
    {TOK_EXACT, tactic_parse_exact},
    {TOK_REWRITE, tactic_parse_rewrite},
    {TOK_REFLEXIVITY, tactic_parse_reflexivity},
    {TOK_ASSUMPTION, tactic_parse_assumption},
    {TOK_SPLIT, tactic_parse_split},
    {TOK_LEFT, tactic_parse_left},
    {TOK_RIGHT, tactic_parse_right},
    {TOK_EXISTS, tactic_parse_exists},
};

#define TACTIC_DISPATCH_TABLE (tactic_dispatch_table)
#define TACTIC_DISPATCH_TABLE_SIZE \
    (sizeof(tactic_dispatch_table) / sizeof(tactic_dispatch_table[0]))

/**
 * Macro to iterate over all entries in the tactic dispatch table.
 *
 * @param entry Loop variable of type TacticDispatchEntry*.
 */
#define TACTIC_FOREACH_ENTRY(entry)                             \
    for (TacticDispatchEntry * (entry) = TACTIC_DISPATCH_TABLE; \
         (entry) < TACTIC_DISPATCH_TABLE + TACTIC_DISPATCH_TABLE_SIZE; ++(entry))

/**
 * Macro to get the key (token type) from a tactic dispatch entry.
 *
 * @param entry Pointer to the TacticDispatchEntry.
 */
#define TACTIC_ENTRY_KEY(entry) ((entry)->type)

/**
 * Macro to get the parse function from a tactic dispatch entry.
 *
 * @param entry Pointer to the TacticDispatchEntry.
 */
#define TACTIC_ENTRY_PARSER(entry) ((entry)->tactic_parse_func)

/**
 * Macro to look up a tactic dispatch entry by token type.
 *
 * @param result_ptr Pointer to store the found TacticDispatchEntry* (or NULL
 * if not found).
 * @param tok_type The TokenType to look up.
 */
#define TACTIC_LOOKUP_ENTRY(result_ptr, tok_type)                           \
    for (bool _tactic_once_ = true; _tactic_once_; _tactic_once_ = false) { \
        *(result_ptr) = NULL;                                               \
        TACTIC_FOREACH_ENTRY(_tactic_e_) {                                  \
            if (TACTIC_ENTRY_KEY(_tactic_e_) == (tok_type)) {               \
                *(result_ptr) = _tactic_e_;                                 \
                break;                                                      \
            }                                                               \
        }                                                                   \
    }

#endif  // TACTIC_PARSER_H
