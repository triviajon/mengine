#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"
#include "src/common/lexer.h"
#include "src/common/parser_base.h"

void fprint_ast(FILE *stream, AST *ast) {
    if (!ast) {
        fprintf(stream, "(null)");
        return;
    }

    switch (ast->tag) {
        case AST_VAR:
            fprintf(stream, "VAR(%s)", ast->value.var.name);
            return;

        case AST_TYPE:
            fprintf(stream, "TYPE");
            return;

        case AST_PROP:
            fprintf(stream, "PROP");
            return;

        case AST_LAMBDA:
            fprintf(stream, "LAMBDA(");
            fprint_ast(stream, ast->value.lambda.binder.type);
            fprintf(stream, ", ");
            fprint_ast(stream, ast->value.lambda.body);
            fprintf(stream, ")");
            return;

        case AST_FORALL:
            fprintf(stream, "FORALL(");
            fprint_ast(stream, ast->value.forall.binder.type);
            fprintf(stream, ", ");
            fprint_ast(stream, ast->value.forall.body);
            fprintf(stream, ")");
            return;

        case AST_APP:
            fprintf(stream, "APP(");
            fprint_ast(stream, ast->value.app.func);
            fprintf(stream, ", ");
            fprint_ast(stream, ast->value.app.arg);
            fprintf(stream, ")");
            return;

        case AST_MATCH:
            fprintf(stream, "MATCH(");
            fprint_ast(stream, ast->value.match.scrutinee);
            fprintf(stream, ", branches=%zu)", ast->value.match.branch_count);
            return;

        case AST_MATCHBRANCH:
            fprintf(stream, "BRANCH(");
            fprintf(stream, "pattern=%s, ", ast->value.matchbranch.pattern
                                       ? ast->value.matchbranch.pattern->name
                                       : "_");
            fprint_ast(stream, ast->value.matchbranch.body);
            fprintf(stream, ")");
            return;

        default:
            fprintf(stream, "UNKNOWN");
            return;
    }
}

void print_ast(AST *ast) {
    fprint_ast(stdout, ast);
}

void debug_print_ast(Parser *p, AST *ast) {
    if (!p->options || !p->options->debug || !p->options->debug__print_ast)
        return;

    fprintf(stderr, MAG "[PARSE]" DIM " ");
    fprint_ast(stderr, ast);
    fprintf(stderr, CRESET "\n");
}

AST *parse_term(Parser *p) {
    AST *term = parse_prefix_term(p);
    return term;
}

AST *parse_prefix_term(Parser *p) {
    if (parser_expect_no_consume(p, TOK_FUN)) {
        return parse_lambda(p);
    }
    if (parser_expect_no_consume(p, TOK_FORALL)) {
        return parse_forall(p);
    }
    if (parser_expect_no_consume(p, TOK_MATCH)) {
        return parse_match(p);
    }
    if (parser_expect_no_consume(p, TOK_LET)) {
        return parse_let(p);
    }
    return parse_application(p);
}

AST *parse_lambda(Parser *p) {
    if (!parser_expect_consume(p, TOK_FUN)) {
        parser_error(p, "Expected 'fun' at start of lambda expression");
    }

    if (!parser_expect_consume(p, TOK_LPAREN)) {
        parser_error(p, "Expected '(' after 'fun'");
    }

    Binder binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_RPAREN)) {
        parser_error(p, "Expected ')' after lambda binder");
    }

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

    if (!parser_expect_consume(p, TOK_LPAREN)) {
        parser_error(p, "Expected '(' after 'forall'");
    }

    Binder binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_RPAREN)) {
        parser_error(p, "Expected ')' after forall binder");
    }

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
    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in binder");
    }
    Token *ident_token = parser_next(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "Expected ':' after binder identifier");
    }

    AST *type = parse_term(p);
    debug_print_ast(p, type);

    Binder binder;
    binder.name = strdup(ident_token->lexeme);
    if (!binder.name) {
        parser_error(p, "Memory allocation failed for binder name");
    }
    lexer_free_token(ident_token);
    binder.type = type;

    return binder;
}

AST *parse_let(Parser *p) {
    if (!parser_expect_consume(p, TOK_LET)) {
        parser_error(p, "Expected 'let' at start of let expression");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in let expression");
    }
    Token *ident_token = parser_next(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "Expected ':' after let identifier");
    }

    AST *type = parse_term(p);
    debug_print_ast(p, type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "Expected ':=' after let type");
    }

    AST *value = parse_term(p);
    debug_print_ast(p, value);

    if (!parser_expect_consume(p, TOK_IN)) {
        parser_error(p, "Expected 'in' after let binding");
    }

    AST *body = parse_term(p);
    debug_print_ast(p, body);

    AST *let_ast = malloc(sizeof(AST));
    let_ast->tag = AST_LET;
    let_ast->value.let.name = strdup(ident_token->lexeme);
    if (!let_ast->value.let.name) {
        parser_error(p, "Memory allocation failed for let name");
    }
    let_ast->value.let.type = type;
    let_ast->value.let.value = value;
    let_ast->value.let.body = body;
    lexer_free_token(ident_token);
    return let_ast;
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
    match_ast->value.match.branch_count = branch_count;
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
    if (!t) {
        return false;
    }
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
    }
    if (parser_expect_consume(p, TOK_TYPE)) {
        AST *type_ast = malloc(sizeof(AST));
        type_ast->tag = AST_TYPE;

        return type_ast;
    }
    if (parser_expect_consume(p, TOK_PROP)) {
        AST *prop_ast = malloc(sizeof(AST));
        prop_ast->tag = AST_PROP;

        return prop_ast;
    }
    if (parser_expect_consume(p, TOK_LPAREN)) {
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
