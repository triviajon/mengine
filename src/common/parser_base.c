#include "src/common/parser_base.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/common/color.h"

void parser_init(Parser *p, Lexer *lx, MEngineOptions *options) {
    p->lx = lx;
    p->current = lexer_next_token(lx);
    // Skip any leading comment tokens
    while (p->current && p->current->type == TOK_COMMENT) {
        Token *comment = p->current;
        p->current = lexer_next_token(lx);
        lexer_free_token(comment);
    }
    p->source = lx->src;
    p->options = options;
    p->error_recovery_set = false;
}

Token *parser_next(Parser *p) {
    Token *old_current = p->current;
    p->current = lexer_next_token(p->lx);
    // Skip any comment tokens
    while (p->current && p->current->type == TOK_COMMENT) {
        Token *comment = p->current;
        p->current = lexer_next_token(p->lx);
        lexer_free_token(comment);
    }
    return old_current;
}

bool parser_eof(Parser *p) { return p->current == NULL || p->current->type == TOK_EOF; }

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
    // Find the start/end of the current line
    int line_start = pos;
    while (line_start > 0 && source[line_start - 1] != '\n') {
        line_start--;
    }

    int line_end = pos;
    while (source[line_end] && source[line_end] != '\n') {
        line_end++;
    }

    // If line is too long, show context around the error
    const int MAX_LINE_LEN = 120;
    int context_start = line_start;
    int context_end = line_end;
    int display_pos = pos - line_start;

    if (line_end - line_start > MAX_LINE_LEN) {
        context_start = pos - (MAX_LINE_LEN / 4);
        if (context_start < line_start) {
            context_start = line_start;
        }
        context_end = context_start + MAX_LINE_LEN;
        if (context_end > line_end) {
            context_end = line_end;
            context_start = context_end - MAX_LINE_LEN;
            if (context_start < line_start) {
                context_start = line_start;
            }
        }
        display_pos = pos - context_start;
    }

    // Print the source snippet
    fprintf(stderr, ERROR);
    for (int i = context_start; i < context_end; i++) {
        fputc(source[i], stderr);
    }
    fprintf(stderr, CRESET "\n");

    // Print the pointer
    for (int i = 0; i < display_pos; i++) {
        char c = source[context_start + i];
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
    fprintf(stderr, ERROR BOLD "Parse Error: " CRESET "%s\n", msg);

    if (p->error_recovery_set) {
        longjmp(p->error_jmp, 1);
    }

    exit(EXIT_FAILURE);
}