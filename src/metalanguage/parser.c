#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void parser_init(Parser *p, Lexer *lx) {
    p->lx = lx;
    p->current = lexer_next_token(lx);
}

Token *parser_next(Parser *p) {
    Token *old_current = p->current;
    p->current = lexer_next_token(p->lx);
    return old_current;
}

Token *parser_peek(Parser *p) { return p->current; }

bool parser_expect_consume(Parser *p, TokenType type) {
    if (p->current && p->current->type == type) {
        Token *t = parser_next(p);
        lexer_free_token(t);
        return true;
    }
    return false;
}

bool parser_expect_no_consume(Parser *p, TokenType type) {
    return p->current && p->current->type == type;
}

void parser_error(Parser *p, const char *msg) {
    int pos = p->current ? p->current->pos : -1;
    fprintf(stderr, "Parse error at position %d: %s\n", pos, msg);
    exit(EXIT_FAILURE);
}

AST *parse_term(Parser *p) { return parse_prefix_term(p); }

AST *parse_prefix_term(Parser *p) {
    if (parser_expect_no_consume(p, TOK_FUN)) {
        return parse_lambda(p);
    } else if (parser_expect_no_consume(p, TOK_FORALL)) {
        return parse_forall(p);
    } else if (parser_expect_no_consume(p, TOK_MATCH)) {
        return parse_match(p);
    } else {
        return parse_application(p);
    }
}

AST *parse_lambda(Parser *p) {
    if (!parser_expect_consume(p, TOK_FUN)) {
        parser_error(p, "Expected 'fun' at start of lambda expression");
    }

    Binder binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_DARROW)) {
        parser_error(p, "Expected '=>' after lambda binder");
    }

    AST *body = parse_term(p);

    AST *lambda_ast = malloc(sizeof(AST));
    lambda_ast->tag = AST_LAMBDA;
    lambda_ast->value.lambda.binder = binder;
    lambda_ast->value.lambda.body = body;

    return lambda_ast;
}

AST *parse_forall(Parser *p) {
    if (!parser_expect_consume(p, TOK_FORALL)) {
        parser_error(p, "Expected 'forall' at start of forall expression");
    }

    Binder binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_COMMA)) {
        parser_error(p, "Expected ',' after forall binder");
    }

    AST *body = parse_term(p);

    AST *forall_ast = malloc(sizeof(AST));
    forall_ast->tag = AST_FORALL;
    forall_ast->value.forall.binder = binder;
    forall_ast->value.forall.body = body;

    return forall_ast;
}

Binder parse_binder(Parser *p) {
    if (!p->current || p->current->type != TOK_IDENT) {
        parser_error(p, "Expected identifier in binder");
    }
    Token *ident_token = parser_next(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "Expected ':' after binder identifier");
    }

    AST *type = parse_term(p);

    Binder binder;
    binder.name = strdup(ident_token->lexeme);
    if (!binder.name) {
        parser_error(p, "Memory allocation failed for binder name");
    }
    lexer_free_token(ident_token);
    binder.type = type;

    return binder;
}

AST *parse_match(Parser *p) {
    if (!parser_expect_consume(p, TOK_MATCH)) {
        parser_error(p, "Expected 'match' at start of match expression");
    }

    AST *match_expr = parse_term(p);

    if (!parser_expect_consume(p, TOK_WITH)) {
        parser_error(p, "Expected 'with' after match expression");
    }

    AST **branches = NULL;
    size_t branch_count = 0;
    while (parser_expect_no_consume(p, TOK_PIPE)) {
        AST *branch = parse_match_branch(p);
        branches = realloc(branches, sizeof(AST *) * (branch_count + 1));
        if (!branches) {
            parser_error(p, "Memory allocation failed for match branches");
        }
        branches[branch_count++] = branch;
    }

    if (!parser_expect_consume(p, TOK_END)) {
        parser_error(p, "Expected 'end' after match branches");
    }

    AST *match_ast = malloc(sizeof(AST));
    match_ast->tag = AST_MATCH;
    match_ast->value.match.scrutinee = match_expr;
    match_ast->value.match.branches = branches;
    return match_ast;
}

AST *parse_match_branch(Parser *p) {
    if (!parser_expect_consume(p, TOK_PIPE)) {
        parser_error(p, "Expected '|' at start of match branch");
    }
    Pattern *pattern = parse_pattern(p);

    if (!parser_expect_consume(p, TOK_DARROW)) {
        parser_error(p, "Expected '=>' after match branch pattern");
    }

    AST *branch_expr = parse_term(p);

    AST *match_branch_ast = malloc(sizeof(AST));
    match_branch_ast->tag = AST_MATCHBRANCH;
    match_branch_ast->value.matchbranch.pattern = pattern;
    match_branch_ast->value.matchbranch.body = branch_expr;

    return match_branch_ast;
}

Pattern *parse_pattern(Parser *p) {
    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in pattern");
    }
    Token *ident_token = parser_next(p);

    Pattern *pattern = malloc(sizeof(Pattern));
    if (!pattern) {
        parser_error(p, "Memory allocation failed for pattern");
    }
    pattern->name = strdup(ident_token->lexeme);
    if (!pattern->name) {
        parser_error(p, "Memory allocation failed for pattern name");
    }
    lexer_free_token(ident_token);

    return pattern;
}

bool is_atomic_start(Token *t) {
    if (!t) return false;
    return t->type == TOK_IDENT || t->type == TOK_LPAREN ||
           t->type == TOK_TYPE || t->type == TOK_PROP;
}

AST *parse_application(Parser *p) {
    AST *func = parse_atomic(p);

    while (true) {
        Token *next_token = parser_peek(p);
        if (!next_token) {
            break;
        }

        // Check if the next token can start an atomic expression
        if (is_atomic_start(next_token)) {
            AST *arg = parse_atomic(p);

            AST *app_ast = malloc(sizeof(AST));
            app_ast->tag = AST_APP;
            app_ast->value.app.func = func;
            app_ast->value.app.arg = arg;

            func = app_ast;  // Update func to the new application
        } else {
            break;  // No more atomic expressions to parse
        }
    }

    return func;
}

AST *parse_atomic(Parser *p) {
    if (parser_peek(p) && parser_peek(p)->type == TOK_IDENT) {
        Token *ident_token = parser_next(p);

        AST *var_ast = malloc(sizeof(AST));
        var_ast->tag = AST_VAR;
        var_ast->value.var.name = strdup(ident_token->lexeme);
        if (!var_ast->value.var.name) {
            parser_error(p, "Memory allocation failed for variable name");
        }
        lexer_free_token(ident_token);

        return var_ast;
    } else if (parser_expect_consume(p, TOK_TYPE)) {
        AST *type_ast = malloc(sizeof(AST));
        type_ast->tag = AST_TYPE;

        return type_ast;
    } else if (parser_expect_consume(p, TOK_PROP)) {
        AST *prop_ast = malloc(sizeof(AST));
        prop_ast->tag = AST_PROP;

        return prop_ast;
    } else if (parser_expect_consume(p, TOK_LPAREN)) {
        AST *inner = parse_term(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "Expected ')' after parenthesized expression");
        }

        return inner;
    } else {
        parser_error(p, "Unexpected token in atomic expression");
        return NULL;
    }
}
