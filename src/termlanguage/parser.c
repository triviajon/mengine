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
            if (ast->value.matchbranch.pattern) {
                fprintf(stream, "pattern=%s", ast->value.matchbranch.pattern->constructor_name);
                for (int i = 0; i < ast->value.matchbranch.pattern->argument_count; i++) {
                    fprintf(stream, " %s", ast->value.matchbranch.pattern->argument_names[i]);
                }
                fprintf(stream, ", ");
            } else {
                fprintf(stream, "pattern=_, ");
            }
            fprint_ast(stream, ast->value.matchbranch.body);
            fprintf(stream, ")");
            return;

        case AST_FIX:
            fprintf(stream, "FIX(%s, binders=%zu, struct=%s, return_type=", ast->value.fix.name,
                    ast->value.fix.binder_count, ast->value.fix.decreasing_arg_name);
            fprint_ast(stream, ast->value.fix.return_type);
            fprintf(stream, ", body=");
            fprint_ast(stream, ast->value.fix.body);
            fprintf(stream, ")");
            return;

        case AST_LET:
            fprintf(stream, "LET(%s : ", ast->value.let.name);
            fprint_ast(stream, ast->value.let.type);
            fprintf(stream, " := ");
            fprint_ast(stream, ast->value.let.value);
            fprintf(stream, " in ");
            fprint_ast(stream, ast->value.let.body);
            fprintf(stream, ")");
            return;

        case AST_PATVAR:
            fprintf(stream, "PATVAR(?%s)", ast->value.patvar.name);
            return;

        case AST_EXPR_REF:
            fprintf(stream, "EXPR_REF(%p)", (void *)ast->value.expr_ref.expr);
            return;

        default:
            fprintf(stream, "UNKNOWN");
            return;
    }
}

void print_ast(AST *ast) { fprint_ast(stdout, ast); }

void debug_print_ast(Parser *p, AST *ast) {
    if (!p->options || !p->options->debug || !p->options->debug__print_ast) {
        return;
    }

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
    if (parser_expect_no_consume(p, TOK_FIX)) {
        return parse_fix(p);
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

    Binder *binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_RPAREN)) {
        parser_error(p, "Expected ')' after lambda binder");
    }

    if (!parser_expect_consume(p, TOK_DARROW)) {
        parser_error(p, "Expected '=>' after lambda binder");
    }

    AST *body = parse_term(p);

    AST *lambda_ast = malloc(sizeof(AST));
    lambda_ast->tag = AST_LAMBDA;
    lambda_ast->value.lambda.binder = *binder;
    lambda_ast->value.lambda.body = body;

    free(binder);

    return lambda_ast;
}

AST *parse_forall(Parser *p) {
    if (!parser_expect_consume(p, TOK_FORALL)) {
        parser_error(p, "Expected 'forall' at start of forall expression");
    }

    if (!parser_expect_consume(p, TOK_LPAREN)) {
        parser_error(p, "Expected '(' after 'forall'");
    }

    Binder *binder = parse_binder(p);

    if (!parser_expect_consume(p, TOK_RPAREN)) {
        parser_error(p, "Expected ')' after forall binder");
    }

    if (!parser_expect_consume(p, TOK_COMMA)) {
        parser_error(p, "Expected ',' after forall binder");
    }

    AST *body = parse_term(p);

    AST *forall_ast = malloc(sizeof(AST));
    forall_ast->tag = AST_FORALL;
    forall_ast->value.forall.binder = *binder;
    forall_ast->value.forall.body = body;

    free(binder);

    return forall_ast;
}

AST *parse_fix(Parser *p) {
    if (!parser_expect_consume(p, TOK_FIX)) {
        parser_error(p, "Expected 'fix' at start of fix expression");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in fix expression");
    }

    Token *ident_token = parser_next(p);

    Binder **binders = NULL;
    size_t binder_count = 0;
    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *binder = parse_binder(p);
        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "Expected ')' after binder");
        }
        binders = realloc(binders, sizeof(Binder *) * (binder_count + 1));
        if (!binders) {
            parser_error(p, "Memory allocation failed for binders");
        }
        binders[binder_count++] = binder;
    }

    char *decreasing_arg_name = parse_decreasing_arg_annotation(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "Expected ':' after decreasing arg annotation");
    }

    AST *return_type = parse_term(p);
    debug_print_ast(p, return_type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "Expected ':=' after return type");
    }

    AST *body = parse_term(p);
    debug_print_ast(p, body);

    AST *fix_ast = malloc(sizeof(AST));
    fix_ast->tag = AST_FIX;
    fix_ast->value.fix.name = strdup(ident_token->lexeme);
    fix_ast->value.fix.binders = binders;
    fix_ast->value.fix.binder_count = binder_count;
    fix_ast->value.fix.decreasing_arg_name = decreasing_arg_name;
    fix_ast->value.fix.return_type = return_type;
    fix_ast->value.fix.body = body;
    return fix_ast;
}

char *parse_decreasing_arg_annotation(Parser *p) {
    if (!parser_expect_consume(p, TOK_LBRACE)) {
        parser_error(p, "Expected '{' at start of decreasing arg annotation");
    }

    if (!parser_expect_consume(p, TOK_STRUCT)) {
        parser_error(p, "Expected 'struct' at start of decreasing arg annotation");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in decreasing arg annotation");
    }
    Token *ident_token = parser_next(p);

    if (!parser_expect_consume(p, TOK_RBRACE)) {
        parser_error(p, "Expected '}' after decreasing arg annotation");
    }
    char *decreasing_arg_name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);
    return decreasing_arg_name;
}

Binder *parse_binder(Parser *p) {
    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "Expected identifier in binder");
    }
    Token *ident_token = parser_next(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "Expected ':' after binder identifier");
    }

    AST *type = parse_term(p);
    debug_print_ast(p, type);

    Binder *binder = malloc(sizeof(Binder));
    binder->name = strdup(ident_token->lexeme);
    if (!binder->name) {
        parser_error(p, "Memory allocation failed for binder name");
    }
    lexer_free_token(ident_token);
    binder->type = type;

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
        parser_error(p, "Expected constructor name in pattern");
    }
    Token *ctor_token = parser_next(p);

    Pattern *pattern = malloc(sizeof(Pattern));
    if (!pattern) {
        parser_error(p, "Memory allocation failed for pattern");
    }

    pattern->constructor_name = strdup(ctor_token->lexeme);
    if (!pattern->constructor_name) {
        parser_error(p, "Memory allocation failed for constructor name");
    }
    lexer_free_token(ctor_token);

    pattern->argument_names = NULL;
    pattern->argument_count = 0;

    while (parser_expect_no_consume(p, TOK_IDENT)) {
        Token *arg_token = parser_next(p);

        pattern->argument_names =
            realloc(pattern->argument_names, sizeof(char *) * (pattern->argument_count + 1));
        if (!pattern->argument_names) {
            parser_error(p, "Memory allocation failed for pattern arguments");
        }

        pattern->argument_names[pattern->argument_count] = strdup(arg_token->lexeme);
        if (!pattern->argument_names[pattern->argument_count]) {
            parser_error(p, "Memory allocation failed for pattern argument name");
        }

        lexer_free_token(arg_token);
        pattern->argument_count++;
    }

    return pattern;
}

bool is_atomic_start(Token *t) {
    if (!t) {
        return false;
    }
    return t->type == TOK_IDENT || t->type == TOK_LPAREN || t->type == TOK_TYPE ||
           t->type == TOK_PROP;
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
    }
    parser_error(p, "Unexpected token in atomic expression");

    return NULL;
}

/* ============================================================================
 * Term pattern parsing (for match goal)
 *
 * Like normal term parsing but additionally handles:
 *   - ?X  → AST_PATVAR("X") — pattern variable that matches any term
 *   - _   → AST_PATVAR("_") — wildcard (matches any term, no binding)
 * ============================================================================ */

static AST *_parse_pattern_atomic(Parser *p);
static AST *_parse_pattern_application(Parser *p);

static bool _is_pattern_atomic_start(Token *t) {
    if (!t) return false;
    return t->type == TOK_IDENT || t->type == TOK_LPAREN || t->type == TOK_TYPE ||
           t->type == TOK_PROP || t->type == TOK_QUESTION;
}

static AST *_parse_pattern_prefix(Parser *p) {
    if (parser_expect_no_consume(p, TOK_FORALL)) {
        parser_expect_consume(p, TOK_FORALL);
        // Pattern forall: forall (x : <pat>) , <pat>
        if (!parser_expect_consume(p, TOK_LPAREN)) {
            parser_error(p, "Expected '(' after 'forall' in pattern");
        }
        // Parse binder with pattern-aware type
        if (!parser_expect_no_consume(p, TOK_IDENT)) {
            parser_error(p, "Expected identifier in forall pattern binder");
        }
        Token *name_tok = parser_next(p);
        char *binder_name = strdup(name_tok->lexeme);
        lexer_free_token(name_tok);
        if (!parser_expect_consume(p, TOK_COLON)) {
            parser_error(p, "Expected ':' in forall pattern binder");
        }
        AST *binder_type = parse_term_pattern(p);
        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "Expected ')' after binder in forall pattern");
        }
        if (!parser_expect_consume(p, TOK_COMMA)) {
            parser_error(p, "Expected ',' after forall binder in pattern");
        }
        AST *body = parse_term_pattern(p);
        AST *forall_ast = malloc(sizeof(AST));
        forall_ast->tag = AST_FORALL;
        forall_ast->value.forall.binder.name = binder_name;
        forall_ast->value.forall.binder.type = binder_type;
        forall_ast->value.forall.body = body;
        return forall_ast;
    }
    return _parse_pattern_application(p);
}

static AST *_parse_pattern_application(Parser *p) {
    AST *func = _parse_pattern_atomic(p);

    while (true) {
        Token *next_token = parser_peek(p);
        if (!next_token || !_is_pattern_atomic_start(next_token)) break;
        AST *arg = _parse_pattern_atomic(p);
        AST *app_ast = malloc(sizeof(AST));
        app_ast->tag = AST_APP;
        app_ast->value.app.func = func;
        app_ast->value.app.arg = arg;
        func = app_ast;
    }

    return func;
}

static AST *_parse_pattern_atomic(Parser *p) {
    // ?X → pattern variable
    if (parser_expect_no_consume(p, TOK_QUESTION)) {
        parser_expect_consume(p, TOK_QUESTION);
        if (!parser_expect_no_consume(p, TOK_IDENT)) {
            parser_error(p, "Expected identifier after '?' in pattern");
        }
        Token *ident_token = parser_next(p);
        AST *patvar = malloc(sizeof(AST));
        patvar->tag = AST_PATVAR;
        patvar->value.patvar.name = strdup(ident_token->lexeme);
        lexer_free_token(ident_token);
        return patvar;
    }

    // identifiers — treat "_" as wildcard pattern variable
    if (parser_peek(p) && parser_peek(p)->type == TOK_IDENT) {
        Token *ident_token = parser_next(p);
        if (strcmp(ident_token->lexeme, "_") == 0) {
            AST *patvar = malloc(sizeof(AST));
            patvar->tag = AST_PATVAR;
            patvar->value.patvar.name = strdup("_");
            lexer_free_token(ident_token);
            return patvar;
        }
        AST *var_ast = malloc(sizeof(AST));
        var_ast->tag = AST_VAR;
        var_ast->value.var.name = strdup(ident_token->lexeme);
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
        AST *inner = parse_term_pattern(p);
        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "Expected ')' in pattern expression");
        }
        return inner;
    }

    parser_error(p, "Unexpected token in pattern expression");
    return NULL;
}

AST *parse_term_pattern(Parser *p) { return _parse_pattern_prefix(p); }
