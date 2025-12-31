#include "src/commandlanguage/command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/lexer.h"
#include "src/common/parser_base.h"

char *decl_keyword_to_string(DeclKeyword kw) {
    switch (kw) {
        case DECL_KW_AXIOM:
            return "Axiom";
        case DECL_KW_VARIABLE:
            return "Variable";
        default:
            return "UnknownDeclKeyword";
    }
}

char *stmt_keyword_to_string(StmtKeyword kw) {
    switch (kw) {
        case STMT_KW_THEOREM:
            return "Theorem";
        case STMT_KW_LEMMA:
            return "Lemma";
        default:
            return "UnknownStmtKeyword";
    }
}

Command *command_parse_command(Parser *p) {
    TokenType tok_type = p->current->type;

    CommandDispatchEntry *entry = NULL;
    CMD_LOOKUP_ENTRY(&entry, tok_type);

    if (entry == NULL) {
        parser_error(p, "Unknown or unsupported command keyword");
    }

    CommandParseFunc fn = CMD_ENTRY_PARSER(entry);
    if (!fn) {
        parser_error(p, "Internal error: null parse function");
    }

    Command *cmd = fn(p);
    return cmd;
}

Command *command_parse_declaration(Parser *p) {
    DeclKeyword kw = command_parse_declaration_keyword(p);
    Binder *binder = command_parse_assumption(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of declaration");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DECLARATION;
    cmd->as.decl.binder = *binder;
    cmd->as.decl.kw = kw;

    free(binder);

    return cmd;
}

DeclKeyword command_parse_declaration_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_AXIOM)) {
        return DECL_KW_AXIOM;
    }
    if (parser_expect_consume(p, TOK_VARIABLE)) {
        return DECL_KW_VARIABLE;
    }
    parser_error(p, "expected 'Axiom' or 'Variable' keyword");

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return DECL_KW_VARIABLE;
}

Binder *command_parse_assumption(Parser *p) { return parse_binder(p); }

/**
 * <constructor> ::= "|" <identifier> ":" <term>
 *
 * @param p Pointer to the Parser.
 * @return InductiveConstructor structure representing the parsed constructor.
 */
InductiveConstructor *command_parse_constructor(Parser *p) {
    if (!parser_expect_consume(p, TOK_PIPE)) {
        parser_error(p, "expected '|' before constructor");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected constructor name after '|'");
    }

    Token *ctor_token = parser_next(p);
    char *ctor_name = strdup(ctor_token->lexeme);
    lexer_free_token(ctor_token);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' after constructor name");
    }

    AST *ctor_type = parse_term(p);
    debug_print_ast(p, ctor_type);

    InductiveConstructor *ctor = malloc(sizeof(InductiveConstructor));
    ctor->name = ctor_name;
    ctor->type = ctor_type;

    return ctor;
}

Command *command_parse_definition(Parser *p) {
    if (!parser_expect_consume(p, TOK_DEFINITION)) {
        parser_error(p, "expected 'Definition'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Definition'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    Binder **params = NULL;
    size_t param_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *b = command_parse_assumption(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "expected ')' after parameter");
        }

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before type");
    }

    AST *type = parse_term(p);
    debug_print_ast(p, type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' in definition");
    }

    AST *body = parse_term(p);
    debug_print_ast(p, body);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DEFINITION;
    cmd->as.defn.name = name;
    cmd->as.defn.params = params;
    cmd->as.defn.param_count = param_count;
    cmd->as.defn.type = type;
    cmd->as.defn.body = body;

    return cmd;
}

Command *command_parse_statement(Parser *p) {
    StmtKeyword kw = command_parse_statement_keyword(p);

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after theorem keyword");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before theorem type");
    }

    AST *ty = parse_term(p);
    debug_print_ast(p, ty);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after theorem statement");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_STATEMENT;
    cmd->as.stmt.kw = kw;
    cmd->as.stmt.name = name;
    cmd->as.stmt.type = ty;

    return cmd;
}

StmtKeyword command_parse_statement_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_THEOREM)) {
        return STMT_KW_THEOREM;
    }
    if (parser_expect_consume(p, TOK_LEMMA)) {
        return STMT_KW_LEMMA;
    }
    parser_error(p, "expected 'Theorem' or 'Lemma' keyword");

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return STMT_KW_LEMMA;
}

Command *command_parse_check(Parser *p) {
    if (!parser_expect_consume(p, TOK_CHECK)) {
        parser_error(p, "expected 'Check'");
    }

    AST *term = parse_term(p);
    debug_print_ast(p, term);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of check command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_CHECK;
    cmd->as.check.term = term;
    return cmd;
}

Command *command_parse_print(Parser *p) {
    if (!parser_expect_consume(p, TOK_PRINT)) {
        parser_error(p, "expected 'Print'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Print'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of print command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_PRINT;
    cmd->as.print.name = name;
    return cmd;
}

Command *command_parse_show(Parser *p) {
    if (!parser_expect_consume(p, TOK_SHOW)) {
        parser_error(p, "expected 'Show'");
    }

    ShowKeyword sh_kw = command_parse_show_type(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after show command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_SHOW;
    cmd->as.show.kw = sh_kw;

    return cmd;
}

ShowKeyword command_parse_show_type(Parser *p) {
    if (parser_expect_consume(p, TOK_CONTEXT)) {
        return SHOW_KW_CONTEXT;
    }

    if (parser_expect_consume(p, TOK_PROOF)) {
        return SHOW_KW_PROOF;
    }

    if (parser_expect_consume(p, TOK_GOAL)) {
        return SHOW_KW_GOAL;
    }

    if (parser_expect_consume(p, TOK_STATE)) {
        return SHOW_KW_STATE;
    }

    parser_error(p, "expected 'Context', 'Proof', 'Goal', or 'State' after 'Show'");

    // Unreachable, but avoids compiler warning.
    return SHOW_KW_STATE;
}

Command *command_parse_inductive(Parser *p) {
    if (!parser_expect_consume(p, TOK_INDUCTIVE)) {
        parser_error(p, "expected 'Inductive'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Inductive'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    // Binders
    Binder **params = NULL;
    size_t param_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *b = parse_binder(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "expected ')' after parameter");
        }

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before inductive type");
    }

    // Type
    AST *type = parse_term(p);
    debug_print_ast(p, type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' after inductive type");
    }

    // Constructors
    InductiveConstructor **constructors = NULL;
    size_t constructor_count = 0;

    while (parser_expect_no_consume(p, TOK_PIPE)) {
        InductiveConstructor *ctor = command_parse_constructor(p);

        constructors =
            realloc(constructors, sizeof(InductiveConstructor *) * (constructor_count + 1));
        constructors[constructor_count] = ctor;
        constructor_count++;
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after inductive definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_INDUCTIVE;
    cmd->as.inductive.name = name;
    cmd->as.inductive.params = params;
    cmd->as.inductive.param_count = param_count;
    cmd->as.inductive.type = type;
    cmd->as.inductive.constructors = constructors;
    cmd->as.inductive.constructor_count = constructor_count;

    return cmd;
}
