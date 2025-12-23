#include "lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"

static inline char peek_char(Lexer *lx) { return lx->src[lx->pos]; }

static void debug_print_token(Lexer *lx, Token *t) {
    if (!lx->options || !lx->options->debug ||
        !lx->options->debug__print_tokens) {
        return;
    }

    char *name;
    switch (t->type) {
        case TOK_IDENT:
            name = "IDENT";
            break;
        case TOK_LPAREN:
            name = "LPAREN";
            break;
        case TOK_RPAREN:
            name = "RPAREN";
            break;
        case TOK_COLON:
            name = "COLON";
            break;
        case TOK_COLON_EQ:
            name = "COLON_EQ";
            break;
        case TOK_COMMA:
            name = "COMMA";
            break;
        case TOK_DOT:
            name = "DOT";
            break;
        case TOK_DARROW:
            name = "DARROW";
            break;
        case TOK_FUN:
            name = "FUN";
            break;
        case TOK_FORALL:
            name = "FORALL";
            break;
        case TOK_TYPE:
            name = "TYPE";
            break;
        case TOK_PROP:
            name = "PROP";
            break;
        case TOK_MATCH:
            name = "MATCH";
            break;
        case TOK_WITH:
            name = "WITH";
            break;
        case TOK_PIPE:
            name = "PIPE";
            break;
        case TOK_END:
            name = "END";
            break;
        case TOK_AXIOM:
            name = "AXIOM";
            break;
        case TOK_VARIABLE:
            name = "VARIABLE";
            break;
        case TOK_DEFINITION:
            name = "DEFINITION";
            break;
        case TOK_THEOREM:
            name = "THEOREM";
            break;
        case TOK_LEMMA:
            name = "LEMMA";
            break;
        case TOK_CHECK:
            name = "CHECK";
            break;
        case TOK_PRINT:
            name = "PRINT";
        case TOK_SHOW:
            name = "SHOW";
            break;
        case TOK_CONTEXT:
            name = "CONTEXT";
            break;
        case TOK_PROOF:
            name = "PROOF";
            break;
        case TOK_GOAL:
            name = "GOAL";
            break;
        case TOK_STATE:
            name = "STATE";
            break;
        case TOK_INDUCTIVE:
            name = "INDUCTIVE";
            break;
        case TOK_ADMITTED:
            name = "ADMITTED";
            break;
        case TOK_INTRO:
            name = "INTRO";
            break;
        case TOK_INTROS:
            name = "INTROS";
            break;
        case TOK_APPLY:
            name = "APPLY";
            break;
        case TOK_EAPPLY:
            name = "EAPPLY";
            break;
        case TOK_EXACT:
            name = "EXACT";
            break;
        case TOK_REWRITE:
            name = "REWRITE";
            break;
        case TOK_LEFT_ARROW:
            name = "LEFT_ARROW";
            break;
        case TOK_REFLEXIVITY:
            name = "REFLEXIVITY";
            break;
        case TOK_ASSUMPTION:
            name = "ASSUMPTION";
            break;
        case TOK_SPLIT:
            name = "SPLIT";
            break;
        case TOK_LEFT:
            name = "LEFT";
            break;
        case TOK_RIGHT:
            name = "RIGHT";
            break;
        case TOK_EXISTS:
            name = "EXISTS";
            break;
        case TOK_COMMENT:
            name = "COMMENT";
            break;
        case TOK_EOF:
            name = "EOF";
            break;
        case TOK_ERROR:
            name = "ERROR";
            break;
        default:
            name = "UNKNOWN";
    }

    fprintf(stderr, YEL "[LEX]" DIM " %-12s at %-4d  %s" CRESET "\n", name,
            t->pos, t->lexeme ? t->lexeme : "");
}

void skip_whitespace(Lexer *lx) {
    while (lx->src[lx->pos] == ' ' || lx->src[lx->pos] == '\t' ||
           lx->src[lx->pos] == '\n' || lx->src[lx->pos] == '\r') {
        lx->pos++;
    }
}

char *skip_comment(Lexer *lx) {
    int depth = 1;
    int start_pos = lx->pos - 1;
    size_t length = 2;

    while (lx->src[lx->pos] != '\0' && depth > 0) {
        if (lx->src[lx->pos] == '(' && lx->src[lx->pos + 1] == '*') {
            depth++;
            lx->pos += 2;
            length += 2;
        } else if (lx->src[lx->pos] == '*' && lx->src[lx->pos + 1] == ')') {
            depth--;
            lx->pos += 2;
            length += 2;
        } else {
            lx->pos++;
            length += 1;
        }
    }

    if (depth != 0) {
        fprintf(stderr, BOLD RED "Unterminated comment\n" CRESET);
        exit(EXIT_FAILURE);
    }

    char *lexeme = (char *)malloc(length + 1);
    strncpy(lexeme, &lx->src[start_pos], length);
    lexeme[length] = '\0';
    return lexeme;
}

char next_char(Lexer *lx) { return lx->src[lx->pos++]; }

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_digit(char c) { return (c >= '0' && c <= '9'); }

bool is_ident_start(char c) { return is_alpha(c) || c == '_'; }

bool is_ident_continue(char c) { return is_ident_start(c) || is_digit(c); }

Token *make_token(TokenType type, int pos, char *lexeme) {
    Token *token = (Token *)malloc(sizeof(Token));
    if (!token) {
        return NULL;
    }
    token->type = type;
    token->pos = pos;
    token->lexeme = lexeme;
    return token;
}

void lexer_init(Lexer *lx, const char *input, MEngineOptions *options) {
    lx->src = input;
    lx->pos = 0;
    lx->options = options;
}

static Token *keyword_or_ident(char *lexeme, size_t start_pos) {
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strcmp(lexeme, keywords[i].name) == 0) {
            free(lexeme);
            return make_token(keywords[i].kind, start_pos, NULL);
        }
    }

    return make_token(TOK_IDENT, start_pos, lexeme);
}

Token *lexer_next_token(Lexer *lx) {
    skip_whitespace(lx);

    char c = next_char(lx);
    Token *token;
    switch (c) {
        case '(': {
            char next = peek_char(lx);
            if (next == '*') {
                int start_pos = lx->pos - 1;
                token = make_token(TOK_COMMENT, start_pos, skip_comment(lx));
                break;
            }

            token = make_token(TOK_LPAREN, lx->pos - 1, NULL);
            break;
        }
        case ')': {
            token = make_token(TOK_RPAREN, lx->pos - 1, NULL);
            break;
        }
        case ':': {
            char next = peek_char(lx);
            if (next == '=') {
                next_char(lx);
                token = make_token(TOK_COLON_EQ, lx->pos - 2, NULL);
                break;
            }
            token = make_token(TOK_COLON, lx->pos - 1, NULL);
            break;
        }
        case ',': {
            return make_token(TOK_COMMA, lx->pos - 1, NULL);
            break;
        }
        case '.': {
            token = make_token(TOK_DOT, lx->pos - 1, NULL);
            break;
        }
        case '=': {
            char next = peek_char(lx);
            if (next == '>') {
                next_char(lx);
                token = make_token(TOK_DARROW, lx->pos - 2, NULL);
                break;
            }
            token = make_token(TOK_ERROR, lx->pos - 1, NULL);
            break;
        }
        case '|': {
            token = make_token(TOK_PIPE, lx->pos - 1, NULL);
            break;
        }
        case '<': {
            char next = peek_char(lx);
            if (next == '-') {
                next_char(lx);
                token = make_token(TOK_LEFT_ARROW, lx->pos - 2, NULL);
                break;
            }
            token = make_token(TOK_ERROR, lx->pos - 1, NULL);
            break;
        }
        case '\0': {
            token = make_token(TOK_EOF, lx->pos - 1, NULL);
            break;
        }
        default: {
            // Handle identifiers and keywords
            if (is_ident_start(c)) {
                int start_pos = lx->pos - 1;
                while (is_ident_continue(lx->src[lx->pos])) {
                    lx->pos++;
                }
                int length = lx->pos - start_pos;
                char *lexeme = (char *)malloc(length + 1);
                strncpy(lexeme, &lx->src[start_pos], length);
                lexeme[length] = '\0';

                // Check for keywords
                token = keyword_or_ident(lexeme, start_pos);
            } else {
                token = make_token(TOK_ERROR, lx->pos - 1, NULL);
            }
        }
    }

    debug_print_token(lx, token);
    return token;
}

Token *lexer_peek_token(Lexer *lx) {
    int saved_pos = lx->pos;
    Token *t = lexer_next_token(lx);
    lx->pos = saved_pos;
    return t;
}

void lexer_free_token(Token *t) {
    if (t == NULL) {
        return;
    }
    if (t->lexeme != NULL) {
        free(t->lexeme);
    }
    free(t);
}