#include "lexer.h"

#include <stdlib.h>
#include <string.h>

void skip_whitespace(Lexer *lx) {
    while (lx->src[lx->pos] == ' ' || lx->src[lx->pos] == '\t' ||
           lx->src[lx->pos] == '\n' || lx->src[lx->pos] == '\r') {
        lx->pos++;
    }
}

char next_char(Lexer *lx) { return lx->src[lx->pos++]; }

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) { return (c >= '0' && c <= '9'); }

bool is_alphanumeric(char c) { return is_alpha(c) || is_digit(c); }

bool is_ident_start(char c) { return is_alpha(c) || c == '_'; }

Token *make_token(TokenType type, int pos, char *lexeme) {
    Token *token = (Token *)malloc(sizeof(Token));
    if (!token) return NULL;
    token->type = type;
    token->pos = pos;
    token->lexeme = lexeme;
    return token;
}

void lexer_init(Lexer *lx, const char *input) {
    lx->src = input;
    lx->pos = 0;
}

Token *lexer_next_token(Lexer *lx) {
    skip_whitespace(lx);

    char c = next_char(lx);
    switch (c) {
        case '(': {
            return make_token(TOK_LPAREN, lx->pos - 1, NULL);
        }
        case ')': {
            return make_token(TOK_RPAREN, lx->pos - 1, NULL);
        }
        case ':': {
            return make_token(TOK_COLON, lx->pos - 1, NULL);
        }
        case ',': {
            return make_token(TOK_COMMA, lx->pos - 1, NULL);
        }
        case '.': {
            return make_token(TOK_DOT, lx->pos - 1, NULL);
        }
        case '=': {
            if (lx->src[lx->pos] == '>') {
                lx->pos++;
                return make_token(TOK_DARROW, lx->pos - 2, NULL);
            } else {
                return make_token(TOK_ERROR, lx->pos - 1, NULL);
            }
        }
        case '|': {
            return make_token(TOK_PIPE, lx->pos - 1, NULL);
        }
        case '\0': {
            return make_token(TOK_EOF, lx->pos - 1, NULL);
        }
        default: {
            // Handle identifiers and keywords
            if (is_ident_start(c)) {
                int start_pos = lx->pos - 1;
                while (is_alphanumeric(lx->src[lx->pos])) {
                    lx->pos++;
                }
                int length = lx->pos - start_pos;
                char *lexeme = (char *)malloc(length + 1);
                strncpy(lexeme, &lx->src[start_pos], length);
                lexeme[length] = '\0';

                // Check for keywords
                if (strcmp(lexeme, "fun") == 0) {
                    free(lexeme);
                    return make_token(TOK_FUN, start_pos, NULL);
                } else if (strcmp(lexeme, "forall") == 0) {
                    free(lexeme);
                    return make_token(TOK_FORALL, start_pos, NULL);
                } else if (strcmp(lexeme, "Type") == 0) {
                    free(lexeme);
                    return make_token(TOK_TYPE, start_pos, NULL);
                } else if (strcmp(lexeme, "Prop") == 0) {
                    free(lexeme);
                    return make_token(TOK_PROP, start_pos, NULL);
                } else if (strcmp(lexeme, "match") == 0) {
                    free(lexeme);
                    return make_token(TOK_MATCH, start_pos, NULL);
                } else if (strcmp(lexeme, "with") == 0) {
                    free(lexeme);
                    return make_token(TOK_WITH, start_pos, NULL);
                } else if (strcmp(lexeme, "end") == 0) {
                    free(lexeme);
                    return make_token(TOK_END, start_pos, NULL);
                } else {
                    return make_token(TOK_IDENT, start_pos, lexeme);
                }
            } else {
                return make_token(TOK_ERROR, lx->pos - 1, NULL);
            }
        }
    }
}

Token *lexer_peek_token(Lexer *lx) {
    int saved_pos = lx->pos;
    Token *t = lexer_next_token(lx);
    lx->pos = saved_pos;
    return t;
}

void lexer_free_token(Token *t) {
    if (t == NULL) return;
    if (t->lexeme != NULL) free(t->lexeme);
    free(t);
}