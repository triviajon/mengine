#include "src/common/parser_base.h"

#include <stdio.h>
#include <stdlib.h>

#include "src/common/color.h"

void parser_init(Parser *p, Lexer *lx, MEngineOptions *options) {
    p->lx = lx;
    p->current = lexer_next_token(lx);
    p->source = lx->src;
    p->options = options;
    p->error_recovery_set = false;
}

Token *parser_next(Parser *p) {
    Token *old_current = p->current;
    p->current = lexer_next_token(p->lx);
    return old_current;
}

bool parser_eof(Parser *p) {
    return p->current == NULL || p->current->type == TOK_EOF;
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

static void print_error_pointer(const char *source, int pos) {
    fprintf(stderr, RED "%s" CRESET, source);

    for (int i = 0; i < pos; i++) {
        char c = source[i];
        if (c == '\t') {
            fputc('\t', stderr);
        } else {
            fputc(' ', stderr);
        }
    }
    fprintf(stderr, "^\n");
}

void parser_error(Parser *p, const char *msg) {
    int pos = p->current ? p->current->pos : -1;

    if (p->source && pos >= 0) {
        print_error_pointer(p->source, pos);
    }
    fprintf(stderr, BOLD RED "Parse Error: " CRESET "%s\n", msg);

    // If error recovery is enabled, jump back to the recovery point
    // Otherwise, exit the program (legacy behavior for non-interactive use)
    if (p->error_recovery_set) {
        longjmp(p->error_jmp, 1);
    }

    exit(EXIT_FAILURE);
}