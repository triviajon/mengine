#include "src/commandlanguage/command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Command *parse_command(Parser *p) {
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

    return fn(p);
}

Command *parse_declaration(Parser *p) {
    DeclKeyword kw = parse_declaration_keyword(p);
    Binder assumption = parse_assumption(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of declaration");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DECLARATION;
    cmd->as.decl.binder = assumption;
    cmd->as.decl.kw = kw;

    return cmd;
}

DeclKeyword parse_declaration_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_AXIOM)) {
        return DECL_KW_AXIOM;
    } else if (parser_expect_consume(p, TOK_VARIABLE)) {
        return DECL_KW_VARIABLE;
    } else {
        parser_error(p, "expected 'Axiom' or 'Variable' keyword");
    }

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return DECL_KW_VARIABLE;
}

Binder parse_assumption(Parser *p) { return parse_binder(p); }

Command *parse_definition(Parser *p) {
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
        Binder b = parse_assumption(p);

        if (!parser_expect_consume(p, TOK_RPAREN))
            parser_error(p, "expected ')' after parameter");

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = malloc(sizeof(Binder));
        *(params[param_count]) = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON))
        parser_error(p, "expected ':' before type");

    AST *type = parse_term(p);

    if (!parser_expect_consume(p, TOK_COLON_EQ))
        parser_error(p, "expected ':=' in definition");

    AST *body = parse_term(p);

    if (!parser_expect_consume(p, TOK_DOT))
        parser_error(p, "expected '.' after definition");

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DEFINITION;
    cmd->as.defn.name = name;
    cmd->as.defn.params = params;
    cmd->as.defn.param_count = param_count;
    cmd->as.defn.type = type;
    cmd->as.defn.body = body;

    return cmd;
}

Command *parse_statement(Parser *p) {
    StmtKeyword kw = parse_statement_keyword(p);

    if (!parser_expect_no_consume(p, TOK_IDENT))
        parser_error(p, "expected identifier after theorem keyword");

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    Binder **params = NULL;
    size_t param_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder b = parse_assumption(p);

        if (!parser_expect_consume(p, TOK_RPAREN))
            parser_error(p, "expected ')' after parameter");

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = malloc(sizeof(Binder));
        *(params[param_count]) = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON))
        parser_error(p, "expected ':' before theorem type");

    AST *ty = parse_term(p);

    if (!parser_expect_consume(p, TOK_DOT))
        parser_error(p, "expected '.' after theorem statement");

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_STATEMENT;
    cmd->as.stmt.kw = kw;
    cmd->as.stmt.name = name;
    cmd->as.stmt.params = params;
    cmd->as.stmt.param_count = param_count;
    cmd->as.stmt.type = ty;

    return cmd;
}

StmtKeyword parse_statement_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_THEOREM)) {
        return STMT_KW_THEOREM;
    } else if (parser_expect_consume(p, TOK_LEMMA)) {
        return STMT_KW_LEMMA;
    } else {
        parser_error(p, "expected 'Theorem' or 'Lemma' keyword");
    }

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return STMT_KW_LEMMA;
}