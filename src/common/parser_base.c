#include "src/common/parser_base.h"

#include <stdio.h>
#include <stdlib.h>

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