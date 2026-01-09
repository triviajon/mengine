#include "src/tacticlanguage/tactic_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *tactic_tag_to_string(TacticTag tag) {
    switch (tag) {
        case TACTIC_ADMITTED:
            return "Admitted";
        case TACTIC_INTRO:
            return "intro";
        case TACTIC_INTROS:
            return "intros";
        case TACTIC_APPLY:
            return "apply";
        case TACTIC_EAPPLY:
            return "eapply";
        case TACTIC_EXACT:
            return "exact";
        case TACTIC_REWRITE:
            return "rewrite";
        case TACTIC_REWRITE_BACKWARD:
            return "rewrite <-";
        case TACTIC_REFLEXIVITY:
            return "reflexivity";
        case TACTIC_ASSUMPTION:
            return "assumption";
        case TACTIC_SPLIT:
            return "split";
        case TACTIC_LEFT:
            return "left";
        case TACTIC_RIGHT:
            return "right";
        case TACTIC_EXISTS:
            return "exists";
        default:
            return "UnknownTactic";
    }
}

Tactic *tactic_parse_proof_command(Parser *p) {
    TokenType tok_type = p->current->type;

    TacticDispatchEntry *entry = NULL;
    TACTIC_LOOKUP_ENTRY(&entry, tok_type);

    if (entry == NULL) {
        parser_error(p, "Unknown or unsupported proof command or tactic keyword");
    }

    TacticParseFunc fn = TACTIC_ENTRY_PARSER(entry);
    if (!fn) {
        parser_error(p, "Internal error: null parse function");
    }

    Tactic *tactic = fn(p);

    return tactic;
}

Tactic *tactic_parse_tactic(Parser *p) {
    TokenType tok_type = p->current->type;

    TacticDispatchEntry *entry = NULL;
    TACTIC_LOOKUP_ENTRY(&entry, tok_type);

    if (entry == NULL) {
        parser_error(p, "Unknown or unsupported tactic keyword");
    }

    TacticParseFunc fn = TACTIC_ENTRY_PARSER(entry);
    if (!fn) {
        parser_error(p, "Internal error: null parse function");
    }

    return fn(p);
}

Tactic *tactic_parse_admitted(Parser *p) {
    if (!parser_expect_consume(p, TOK_ADMITTED)) {
        parser_error(p, "expected 'Admitted'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of Admitted");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_ADMITTED;

    return tactic;
}

Tactic *tactic_parse_intro(Parser *p) {
    if (!parser_expect_consume(p, TOK_INTRO)) {
        parser_error(p, "expected 'intro'");
    }

    char *name = NULL;

    // Optional identifier
    if (parser_expect_no_consume(p, TOK_IDENT)) {
        Token *ident_token = parser_next(p);
        name = strdup(ident_token->lexeme);
        lexer_free_token(ident_token);
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of intro");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_INTRO;
    tactic->as.intro.name = name;

    return tactic;
}

Tactic *tactic_parse_intros(Parser *p) {
    if (!parser_expect_consume(p, TOK_INTROS)) {
        parser_error(p, "expected 'intros'");
    }

    char **names = NULL;
    size_t name_count = 0;

    // Zero or more identifiers
    while (parser_expect_no_consume(p, TOK_IDENT)) {
        Token *ident_token = parser_next(p);

        names = realloc(names, sizeof(char *) * (name_count + 1));
        names[name_count] = strdup(ident_token->lexeme);
        lexer_free_token(ident_token);
        name_count++;
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of intros");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_INTROS;
    tactic->as.intros.names = names;
    tactic->as.intros.name_count = name_count;

    return tactic;
}

Tactic *tactic_parse_apply(Parser *p) {
    if (!parser_expect_consume(p, TOK_APPLY)) {
        parser_error(p, "expected 'apply'");
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of apply");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_APPLY;
    tactic->as.apply.lemma = lemma;

    return tactic;
}

Tactic *tactic_parse_eapply(Parser *p) {
    if (!parser_expect_consume(p, TOK_EAPPLY)) {
        parser_error(p, "expected 'eapply'");
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of eapply");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_EAPPLY;
    tactic->as.eapply.lemma = lemma;

    return tactic;
}

Tactic *tactic_parse_exact(Parser *p) {
    if (!parser_expect_consume(p, TOK_EXACT)) {
        parser_error(p, "expected 'exact'");
    }

    AST *proof_term = parse_term(p);
    debug_print_ast(p, proof_term);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of exact");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_EXACT;
    tactic->as.exact.proof_term = proof_term;

    return tactic;
}

Tactic *tactic_parse_rewrite(Parser *p) {
    if (!parser_expect_consume(p, TOK_REWRITE)) {
        parser_error(p, "expected 'rewrite'");
    }

    bool backward = false;

    // Check for optional "<-"
    if (parser_expect_consume(p, TOK_LEFT_ARROW)) {
        backward = true;
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    // Expect "with"
    if (!parser_expect_consume(p, TOK_WITH)) {
        parser_error(p, "Expected 'with' after rewrite lemma");
    }

    AST *equiv_proof = parse_term(p);
    debug_print_ast(p, equiv_proof);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of rewrite");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = backward ? TACTIC_REWRITE_BACKWARD : TACTIC_REWRITE;
    tactic->as.rewrite.lemma = lemma;
    tactic->as.rewrite.equiv_proof = equiv_proof;
    tactic->as.rewrite.backward = backward;

    return tactic;
}

Tactic *tactic_parse_erewrite(Parser *p) {
    if (!parser_expect_consume(p, TOK_EREWRITE)) {
        parser_error(p, "expected 'erewrite'");
    }

    bool backward = false;

    // Check for optional "<-"
    if (parser_expect_consume(p, TOK_LEFT_ARROW)) {
        backward = true;
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    // Expect "with"
    if (!parser_expect_consume(p, TOK_WITH)) {
        parser_error(p, "Expected 'with' after rewrite lemma");
    }

    AST *equiv_proof = parse_term(p);
    debug_print_ast(p, equiv_proof);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of rewrite");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = backward ? TACTIC_EREWRITE_BACKWARD : TACTIC_EREWRITE;
    tactic->as.rewrite.lemma = lemma;
    tactic->as.rewrite.equiv_proof = equiv_proof;
    tactic->as.rewrite.backward = backward;

    return tactic;
}

Tactic *tactic_parse_reflexivity(Parser *p) {
    if (!parser_expect_consume(p, TOK_REFLEXIVITY)) {
        parser_error(p, "expected 'reflexivity'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of reflexivity");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_REFLEXIVITY;

    return tactic;
}

Tactic *tactic_parse_assumption(Parser *p) {
    if (!parser_expect_consume(p, TOK_ASSUMPTION)) {
        parser_error(p, "expected 'assumption'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of assumption");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_ASSUMPTION;

    return tactic;
}

Tactic *tactic_parse_split(Parser *p) {
    if (!parser_expect_consume(p, TOK_SPLIT)) {
        parser_error(p, "expected 'split'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of split");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_SPLIT;

    return tactic;
}

Tactic *tactic_parse_left(Parser *p) {
    if (!parser_expect_consume(p, TOK_LEFT)) {
        parser_error(p, "expected 'left'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of left");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_LEFT;

    return tactic;
}

Tactic *tactic_parse_right(Parser *p) {
    if (!parser_expect_consume(p, TOK_RIGHT)) {
        parser_error(p, "expected 'right'");
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of right");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_RIGHT;

    return tactic;
}

Tactic *tactic_parse_exists(Parser *p) {
    if (!parser_expect_consume(p, TOK_EXISTS)) {
        parser_error(p, "expected 'exists'");
    }

    AST *witness = parse_term(p);
    debug_print_ast(p, witness);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of exists");
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_EXISTS;
    tactic->as.exists.witness = witness;

    return tactic;
}
