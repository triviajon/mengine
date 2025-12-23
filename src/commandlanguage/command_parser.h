#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "src/common/lexer.h"
#include "src/metalanguage/parser.h"

typedef enum {
    CMD_DECLARATION,
    CMD_DEFINITION,
    CMD_STATEMENT,
    CMD_CHECK,
    CMD_PRINT,
    CMD_INDUCTIVE,
    CMD_SHOW
} CommandTag;

typedef enum { DECL_KW_AXIOM, DECL_KW_VARIABLE } DeclKeyword;

typedef struct {
    DeclKeyword kw;
} DeclKeywordCmd;

char *decl_keyword_to_string(DeclKeyword kw);

typedef enum { STMT_KW_THEOREM, STMT_KW_LEMMA } StmtKeyword;

typedef struct {
    StmtKeyword kw;
} StmtKeywordCmd;

char *stmt_keyword_to_string(StmtKeyword kw);

typedef enum {
    SHOW_KW_CONTEXT,
    SHOW_KW_PROOF,
    SHOW_KW_GOAL,
    SHOW_KW_STATE
} ShowKeyword;

typedef struct {
    ShowKeyword kw;
} ShowCmd;

typedef struct {
    DeclKeyword kw;
    Binder binder;
} DeclarationCmd;

typedef struct {
    AST *term;
} CheckCmd;

typedef struct {
    char *name;
} PrintCmd;

typedef struct {
    char *name;
    Binder **params;
    size_t param_count;
    AST *type;
    AST *body;
} DefinitionCmd;

typedef struct {
    StmtKeyword kw;
    char *name;
    AST *type;
} StatementCmd;

typedef struct {
    char *name;  // Constructor name
    AST *type;   // Constructor type
} InductiveConstructor;

typedef struct {
    char *name;       // Inductive type name
    Binder **params;  // Parameters (e.g., A : Type, R : A -> A -> Prop)
    size_t param_count;
    AST *type;                            // Return type (e.g., Prop, Set, Type)
    InductiveConstructor **constructors;  // List of constructors
    size_t constructor_count;
} InductiveCmd;

typedef struct Command {
    CommandTag tag;
    union {
        ShowCmd show;
        CheckCmd check;
        PrintCmd print;
        DeclarationCmd decl;
        DefinitionCmd defn;
        StatementCmd stmt;
        InductiveCmd inductive;
    } as;
} Command;

/**
 * <command> ::= <declaration> | <definition> | <statement> | <check>
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed command.
 */
Command *command_parse_command(Parser *p);

/**
 * <declaration> ::= <declaration_keyword> <assumption> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed declaration.
 */
Command *command_parse_declaration(Parser *p);

/**
 * <declaration_keyword> ::= "Axiom" | "Variable"
 *
 * @param p Pointer to the Parser.
 * @return DeclKeyword representing the parsed declaration keyword.
 */
DeclKeyword command_parse_declaration_keyword(Parser *p);

/**
 * <assumption> ::= <binder>
 *
 * @param p Pointer to the Parser.
 * @return Binder structure representing the parsed assumption.
 */
Binder command_parse_assumption(Parser *p);

/**
 * <definition> ::= "Definition" <identifier> { "(" <binder> ")" }
 *                  ":" <term>
 *                  ":=" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed definition.
 */
Command *command_parse_definition(Parser *p);

/**
 * <statement> ::= <statement_keyword> <identifier>
 *                  ":" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed statement.
 */
Command *command_parse_statement(Parser *p);

/**
 * <statement_keyword> ::= "Theorem" | "Lemma"
 *
 * @param p Pointer to the Parser.
 * @return StmtKeyword representing the parsed statement keyword.
 */
StmtKeyword command_parse_statement_keyword(Parser *p);

/**
 * <check> ::= "Check" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed check command.
 */
Command *command_parse_check(Parser *p);

/**
 * <print> ::= "Print" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed print command.
 */
Command *command_parse_print(Parser *p);

/**
 * <show> ::= "Show" <show_keyword>
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed show command.
 */
Command *command_parse_show(Parser *p);

/**
 * <show_keyword> ::= "Context" | "Proof" | "Goal" | "State"
 *
 * @param p Pointer to the Parser.
 * @return ShowKeyword representing the parsed show keyword.
 */
ShowKeyword command_parse_show_type(Parser *p);

/**
 * <constructor> ::= "|" <identifier> ":" <term>
 *
 * @param p Pointer to the Parser.
 * @return InductiveConstructor structure representing the parsed constructor.
 */
InductiveConstructor *command_parse_constructor(Parser *p);

/**
 * <inductive> ::= "Inductive" <identifier> { "(" <binder> ")" }
 *                  ":" <term>
 *                  ":=" { <constructor> } "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed inductive command.
 */
Command *command_parse_inductive(Parser *p);

typedef Command *(*CommandParseFunc)(Parser *p);

typedef struct {
    TokenType type;
    CommandParseFunc parse_func;
} CommandDispatchEntry;

static CommandDispatchEntry command_dispatch_table[] = {
    {TOK_AXIOM, command_parse_declaration},
    {TOK_VARIABLE, command_parse_declaration},
    {TOK_DEFINITION, command_parse_definition},
    {TOK_THEOREM, command_parse_statement},
    {TOK_LEMMA, command_parse_statement},
    {TOK_CHECK, command_parse_check},
    {TOK_PRINT, command_parse_print},
    {TOK_INDUCTIVE, command_parse_inductive},
    {TOK_SHOW, command_parse_show}};

#define CMD_DISPATCH_TABLE (command_dispatch_table)
#define CMD_DISPATCH_TABLE_SIZE \
    (sizeof(command_dispatch_table) / sizeof(command_dispatch_table[0]))

/**
 * Macro to iterate over all entries in the command dispatch table.
 *
 * @param entry Loop variable of type CommandDispatchEntry*.
 */
#define CMD_FOREACH_ENTRY(entry)                              \
    for (CommandDispatchEntry * (entry) = CMD_DISPATCH_TABLE; \
         (entry) < CMD_DISPATCH_TABLE + CMD_DISPATCH_TABLE_SIZE; ++(entry))

/**
 * Macro to get the key (token type) from a command dispatch entry.
 *
 * @param entry Pointer to the CommandDispatchEntry.
 */
#define CMD_ENTRY_KEY(entry) ((entry)->type)

/**
 * Macro to get the parse function from a command dispatch entry.
 *
 * @param entry Pointer to the CommandDispatchEntry.
 */
#define CMD_ENTRY_PARSER(entry) ((entry)->parse_func)

/**
 * Macro to look up a command dispatch entry by token type.
 *
 * @param result_ptr Pointer to store the found CommandDispatchEntry* (or NULL
 * if not found).
 * @param tok_type The CommandTokenType to look up.
 */
#define CMD_LOOKUP_ENTRY(result_ptr, tok_type)                     \
    for (bool _cmd_once_ = true; _cmd_once_; _cmd_once_ = false) { \
        *(result_ptr) = NULL;                                      \
        CMD_FOREACH_ENTRY(_cmd_e_) {                               \
            if (CMD_ENTRY_KEY(_cmd_e_) == (tok_type)) {            \
                *(result_ptr) = _cmd_e_;                           \
                break;                                             \
            }                                                      \
        }                                                          \
    }

#endif  // COMMAND_PARSER_H
