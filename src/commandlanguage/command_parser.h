#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include "src/metalanguage/parser.h"

typedef enum {
    CMD_DECLARATION,
    CMD_DEFINITION,
    CMD_STATEMENT,
    CMD_CHECK,
    CMD_DECL_KEYWORD,
    CMD_STMT_KEYWORD
} CommandTag;

typedef enum { DECL_KW_AXIOM, DECL_KW_VARIABLE } DeclKeyword;

typedef enum { STMT_KW_THEOREM, STMT_KW_LEMMA } StmtKeyword;

typedef struct {
    DeclKeyword kw;
} DeclKeywordCmd;

typedef struct {
    StmtKeyword kw;
} StmtKeywordCmd;

typedef struct {
    DeclKeyword kw;
    Binder binder;
} DeclarationCmd;

char *decl_keyword_to_string(DeclKeyword kw);
char *stmt_keyword_to_string(StmtKeyword kw);

typedef struct {
    AST *term;
} CheckCmd;

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

typedef struct Command {
    CommandTag tag;
    union {
        DeclKeywordCmd decl_kw;
        StmtKeywordCmd stmt_kw;
        CheckCmd check;
        DeclarationCmd decl;
        DefinitionCmd defn;
        StatementCmd stmt;
    } as;
} Command;

/**
 * <command> ::= <declaration> | <definition> | <statement> | <check>
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed command.
 */
Command *parse_command(Parser *p);

/**
 * <declaration> ::= <declaration_keyword> <assumption> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed declaration.
 */
Command *parse_declaration(Parser *p);

/**
 * <declaration_keyword> ::= "Axiom" | "Variable"
 *
 * @param p Pointer to the Parser.
 * @return DeclKeyword representing the parsed declaration keyword.
 */
DeclKeyword parse_declaration_keyword(Parser *p);

/**
 * <assumption> ::= <binder>
 *
 * @param p Pointer to the Parser.
 * @return Binder structure representing the parsed assumption.
 */
Binder parse_assumption(Parser *p);

/**
 * <definition> ::= "Definition" <identifier> { "(" <binder> ")" }
 *                  ":" <term>
 *                  ":=" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed definition.
 */
Command *parse_definition(Parser *p);

/**
 * <statement> ::= <statement_keyword> <identifier>
 *                  ":" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed statement.
 */
Command *parse_statement(Parser *p);

/**
 * <statement_keyword> ::= "Theorem" | "Lemma"
 *
 * @param p Pointer to the Parser.
 * @return StmtKeyword representing the parsed statement keyword.
 */
StmtKeyword parse_statement_keyword(Parser *p);

/**
 * <check> ::= "Check" <term> "."
 *
 * @param p Pointer to the Parser.
 * @return Command structure representing the parsed check command.
 */
Command *parse_check(Parser *p);

typedef Command *(*CommandParseFunc)(Parser *p);

typedef struct {
    TokenType type;
    CommandParseFunc parse_func;
} CommandDispatchEntry;

static CommandDispatchEntry command_dispatch_table[] = {
    {TOK_AXIOM, parse_declaration},     {TOK_VARIABLE, parse_declaration},
    {TOK_DEFINITION, parse_definition}, {TOK_THEOREM, parse_statement},
    {TOK_LEMMA, parse_statement},       {TOK_CHECK, parse_check},
};

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
