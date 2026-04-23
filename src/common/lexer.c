#include "src/common/lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/color.h"

static inline char peek_char(Lexer *lx) { return lx->src[lx->pos]; }

static const char *token_type_name(TokenType t) {
    static const char *names[NUM_TOKEN_TYPES] = {
        [TOK_IDENT] = "IDENT",
        [TOK_LPAREN] = "LPAREN",
        [TOK_RPAREN] = "RPAREN",
        [TOK_LBRACE] = "LBRACE",
        [TOK_RBRACE] = "RBRACE",
        [TOK_COLON] = "COLON",
        [TOK_COLON_EQ] = "COLON_EQ",
        [TOK_COMMA] = "COMMA",
        [TOK_DOT] = "DOT",

        [TOK_DARROW] = "DARROW",
        [TOK_PIPE] = "PIPE",

#define X(lexeme, tok, dbg) [tok] = dbg,
#include "src/common/token_keywords.def"
#undef X

        [TOK_LEFT_ARROW] = "LEFT_ARROW",
        [TOK_SEMICOLON] = "SEMICOLON",
        [TOK_DOUBLE_PIPE] = "DOUBLE_PIPE",
        [TOK_LBRACKET] = "LBRACKET",
        [TOK_RBRACKET] = "RBRACKET",

        [TOK_COMMENT] = "COMMENT",
        [TOK_EOF] = "EOF",
        [TOK_ERROR] = "ERROR",
    };

    if ((unsigned)t >= (unsigned)NUM_TOKEN_TYPES || names[t] == NULL) {
        return "UNKNOWN";
    }
    return names[t];
}

static void debug_print_token(Lexer *lx, Token *t) {
    if (!lx->options || !lx->options->debug || !lx->options->debug__print_tokens) {
        return;
    }

    fprintf(stderr, YEL "[LEX]" DIM " %-12s at %-4d  %s" CRESET "\n", token_type_name(t->type),
            t->pos, t->lexeme ? t->lexeme : "");
}

void skip_whitespace(Lexer *lx) {
    while (lx->src[lx->pos] == ' ' || lx->src[lx->pos] == '\t' || lx->src[lx->pos] == '\n' ||
           lx->src[lx->pos] == '\r') {
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

bool is_alpha(char c) { return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) != 0; }

bool is_digit(char c) { return (c >= '0' && c <= '9') != 0; }

bool is_ident_start(char c) { return (is_alpha(c) || c == '_' || c == '\'') != 0; }

bool is_ident_continue(char c) { return (is_ident_start(c) || is_digit(c)) != 0; }

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
        case '{': {
            token = make_token(TOK_LBRACE, lx->pos - 1, NULL);
            break;
        }
        case '}': {
            token = make_token(TOK_RBRACE, lx->pos - 1, NULL);
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
            char next = peek_char(lx);
            if (next == '|') {
                next_char(lx);
                token = make_token(TOK_DOUBLE_PIPE, lx->pos - 2, NULL);
                break;
            }
            if (next == '-') {
                next_char(lx);
                token = make_token(TOK_TURNSTILE, lx->pos - 2, NULL);
                break;
            }
            token = make_token(TOK_PIPE, lx->pos - 1, NULL);
            break;
        }
        case ';': {
            token = make_token(TOK_SEMICOLON, lx->pos - 1, NULL);
            break;
        }
        case '[': {
            token = make_token(TOK_LBRACKET, lx->pos - 1, NULL);
            break;
        }
        case ']': {
            token = make_token(TOK_RBRACKET, lx->pos - 1, NULL);
            break;
        }
        case '?': {
            token = make_token(TOK_QUESTION, lx->pos - 1, NULL);
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