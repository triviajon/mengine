#include "src/common/parser_base.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"

void parser_init(Parser *p, Lexer *lx, MEngineOptions *options) {
    p->lx = lx;
    p->current = lexer_next_token(lx);
    p->source = lx->src;
    p->options = options;
    p->error_recovery = (options->execution_type == MENGINE_EXECUTION_TYPE_REPL);
    p->pending_comments = NULL;
    p->pending_comment_count = 0;
    p->pending_comment_capacity = 0;

    // Print leading comments
    while (p->current && p->current->type == TOK_COMMENT) {
        Token *comment = p->current;
        if (comment->lexeme) {
            MPRINT(p->options->quiet, stdout, DIMTEXT "%s\n" CRESET, comment->lexeme);
        }
        p->current = lexer_next_token(p->lx);
        lexer_free_token(comment);
    }
}

static void add_pending_comment(Parser *p, const char *comment) {
    if (p->pending_comment_count >= p->pending_comment_capacity) {
        p->pending_comment_capacity =
            (p->pending_comment_capacity == 0) ? 4 : p->pending_comment_capacity * 2;
        p->pending_comments =
            realloc(p->pending_comments, p->pending_comment_capacity * sizeof(char *));
    }
    p->pending_comments[p->pending_comment_count++] = strdup(comment);
}

Token *parser_next(Parser *p) {
    Token *old_current = p->current;
    p->current = lexer_next_token(p->lx);
    while (p->current && p->current->type == TOK_COMMENT) {
        Token *comment = p->current;
        if (comment->lexeme) {
            add_pending_comment(p, comment->lexeme);
        }
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

    if (p->error_recovery) {
        longjmp(p->error_jmp, 1);
    }

    exit(EXIT_FAILURE);
}

void parser_flush_comments(Parser *p) {
    for (int i = 0; i < p->pending_comment_count; i++) {
        MPRINT(p->options->quiet, stdout, DIMTEXT "%s\n" CRESET, p->pending_comments[i]);
        free(p->pending_comments[i]);
    }
    p->pending_comment_count = 0;
}