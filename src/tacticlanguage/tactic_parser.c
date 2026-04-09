#include "src/tacticlanguage/tactic_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/lexer.h"
#include "src/common/parser_base.h"
#include "src/tacticlanguage/tactic_ast.h"

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

/* ============================================================================
 * Primitive tactic parsers (do NOT consume trailing '.')
 * ============================================================================ */

static Tactic *_parse_rewrite(Parser *p) {
    if (!parser_expect_consume(p, TOK_REWRITE)) {
        parser_error(p, "expected 'rewrite'");
    }

    bool backward = false;
    if (parser_expect_consume(p, TOK_LEFT_ARROW)) {
        backward = true;
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    if (!parser_expect_consume(p, TOK_WITH)) {
        parser_error(p, "Expected 'with' after rewrite lemma");
    }

    AST *equiv_proof = parse_term(p);
    debug_print_ast(p, equiv_proof);

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = backward ? TACTIC_REWRITE_BACKWARD : TACTIC_REWRITE;
    tactic->as.rewrite.lemma = lemma;
    tactic->as.rewrite.equiv_proof = equiv_proof;
    tactic->as.rewrite.backward = backward;
    return tactic;
}

static Tactic *_parse_erewrite(Parser *p) {
    if (!parser_expect_consume(p, TOK_EREWRITE)) {
        parser_error(p, "expected 'erewrite'");
    }

    bool backward = false;
    if (parser_expect_consume(p, TOK_LEFT_ARROW)) {
        backward = true;
    }

    AST *lemma = parse_term(p);
    debug_print_ast(p, lemma);

    if (!parser_expect_consume(p, TOK_WITH)) {
        parser_error(p, "Expected 'with' after rewrite lemma");
    }

    AST *equiv_proof = parse_term(p);
    debug_print_ast(p, equiv_proof);

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = backward ? TACTIC_EREWRITE_BACKWARD : TACTIC_EREWRITE;
    tactic->as.rewrite.lemma = lemma;
    tactic->as.rewrite.equiv_proof = equiv_proof;
    tactic->as.rewrite.backward = backward;
    return tactic;
}

static Tactic *_parse_cbv(Parser *p) {
    if (!parser_expect_consume(p, TOK_CBV)) {
        parser_error(p, "expected 'cbv'");
    }

    char **rules = NULL;
    size_t rules_count = 0;

    while (parser_expect_no_consume(p, TOK_IDENT)) {
        Token *ident_token = parser_next(p);
        rules = realloc(rules, sizeof(char *) * (rules_count + 1));
        rules[rules_count] = strdup(ident_token->lexeme);
        lexer_free_token(ident_token);
        rules_count++;
    }

    Tactic *tactic = malloc(sizeof(Tactic));
    tactic->tag = TACTIC_CBV;
    tactic->as.cbv.rules = rules;
    tactic->as.cbv.rules_count = rules_count;
    return tactic;
}

/* ============================================================================
 * Internal dispatch table for primitive tactics
 * ============================================================================ */

typedef Tactic *(*InternalTacticParseFunc)(Parser *p);

typedef struct {
    TokenType type;
    InternalTacticParseFunc parse_func;
} InternalTacticDispatchEntry;

static InternalTacticDispatchEntry internal_dispatch_table[] = {
    {TOK_REWRITE, _parse_rewrite},
    {TOK_EREWRITE, _parse_erewrite},
    {TOK_CBV, _parse_cbv},
};

#define INTERNAL_DISPATCH_SIZE \
    (sizeof(internal_dispatch_table) / sizeof(internal_dispatch_table[0]))

static InternalTacticParseFunc _lookup_primitive(TokenType tok) {
    for (size_t i = 0; i < INTERNAL_DISPATCH_SIZE; i++) {
        if (internal_dispatch_table[i].type == tok) {
            return internal_dispatch_table[i].parse_func;
        }
    }
    return NULL;
}

/* ============================================================================
 * Tactic expression parser (precedence climbing)
 *
 * Precedence (lowest to highest):
 *   1. orelse:  tac1 || tac2
 *   2. seq:     tac1 ; tac2
 *   3. atom:    primitive | try | repeat | first | idtac | fail | (expr)
 * ============================================================================ */

static TacticExpr *_parse_tactic_expr(Parser *p);
static TacticExpr *_parse_tactic_seq(Parser *p);
static TacticExpr *_parse_tactic_atom(Parser *p);

static TacticExpr *_parse_tactic_expr(Parser *p) {
    TacticExpr *left = _parse_tactic_seq(p);

    while (parser_expect_no_consume(p, TOK_DOUBLE_PIPE)) {
        parser_expect_consume(p, TOK_DOUBLE_PIPE);
        TacticExpr *right = _parse_tactic_seq(p);
        left = tactic_expr_orelse(left, right);
    }

    return left;
}

static TacticExpr *_parse_tactic_seq(Parser *p) {
    TacticExpr *left = _parse_tactic_atom(p);

    while (parser_expect_no_consume(p, TOK_SEMICOLON)) {
        parser_expect_consume(p, TOK_SEMICOLON);
        TacticExpr *right = _parse_tactic_atom(p);
        left = tactic_expr_seq(left, right);
    }

    return left;
}

static TacticExpr *_parse_tactic_atom(Parser *p) {
    TokenType tok = p->current->type;

    // try <tactic_atom>
    if (tok == TOK_TRY) {
        parser_expect_consume(p, TOK_TRY);
        TacticExpr *body = _parse_tactic_atom(p);
        return tactic_expr_try(body);
    }

    // repeat <tactic_atom>
    if (tok == TOK_REPEAT) {
        parser_expect_consume(p, TOK_REPEAT);
        TacticExpr *body = _parse_tactic_atom(p);
        return tactic_expr_repeat(body);
    }

    // first [ tac1 | tac2 | ... ]
    if (tok == TOK_FIRST) {
        parser_expect_consume(p, TOK_FIRST);
        if (!parser_expect_consume(p, TOK_LBRACKET)) {
            parser_error(p, "Expected '[' after 'first'");
        }

        TacticExpr **alts = NULL;
        size_t count = 0;

        alts = realloc(alts, sizeof(TacticExpr *) * (count + 1));
        alts[count++] = _parse_tactic_expr(p);

        while (parser_expect_no_consume(p, TOK_PIPE)) {
            parser_expect_consume(p, TOK_PIPE);
            alts = realloc(alts, sizeof(TacticExpr *) * (count + 1));
            alts[count++] = _parse_tactic_expr(p);
        }

        if (!parser_expect_consume(p, TOK_RBRACKET)) {
            parser_error(p, "Expected ']' after 'first' alternatives");
        }

        return tactic_expr_first(alts, count);
    }

    // idtac
    if (tok == TOK_IDTAC) {
        parser_expect_consume(p, TOK_IDTAC);
        return tactic_expr_idtac();
    }

    // fail
    if (tok == TOK_FAIL) {
        parser_expect_consume(p, TOK_FAIL);
        return tactic_expr_fail();
    }

    // let x := <tactic_expr> in <tactic_expr>
    if (tok == TOK_LET) {
        parser_expect_consume(p, TOK_LET);
        if (!parser_expect_no_consume(p, TOK_IDENT)) {
            parser_error(p, "Expected identifier after 'let'");
        }
        Token *name_tok = parser_next(p);
        char *name = strdup(name_tok->lexeme);
        lexer_free_token(name_tok);

        if (!parser_expect_consume(p, TOK_COLON_EQ)) {
            parser_error(p, "Expected ':=' after let binding name");
        }

        TacticExpr *rhs = _parse_tactic_atom(p);

        if (!parser_expect_consume(p, TOK_IN)) {
            parser_error(p, "Expected 'in' after let binding value");
        }

        TacticExpr *body = _parse_tactic_expr(p);

        return tactic_expr_let(name, rhs, body);
    }

    // goal_type - returns the type of the current goal
    if (tok == TOK_GOAL_TYPE) {
        parser_expect_consume(p, TOK_GOAL_TYPE);
        return tactic_expr_goal_type();
    }

    // type_of <term> - returns the type of a term
    if (tok == TOK_TYPE_OF) {
        parser_expect_consume(p, TOK_TYPE_OF);
        AST *term = parse_atomic(p);
        return tactic_expr_type_of(term);
    }

    // mk_hole <type> - creates a hole of the given type, returns it
    if (tok == TOK_MK_HOLE) {
        parser_expect_consume(p, TOK_MK_HOLE);
        AST *type = parse_atomic(p);
        return tactic_expr_mk_hole(type);
    }

    // fill <hole> <term> - fills a hole with a term
    if (tok == TOK_FILL) {
        parser_expect_consume(p, TOK_FILL);
        AST *hole = parse_atomic(p);
        AST *term = parse_atomic(p);
        return tactic_expr_fill(hole, term);
    }

    // subst <new> <body> <old> - substitution body[old := new]
    if (tok == TOK_SUBST) {
        parser_expect_consume(p, TOK_SUBST);
        AST *new_term = parse_atomic(p);
        AST *body = parse_atomic(p);
        AST *old_var = parse_atomic(p);
        return tactic_expr_subst(new_term, body, old_var);
    }

    // eunify <lemma> - existential unification against current goal
    if (tok == TOK_EUNIFY) {
        parser_expect_consume(p, TOK_EUNIFY);
        AST *lemma = parse_atomic(p);
        return tactic_expr_eunify(lemma);
    }

    // current_goal - returns the current goal hole as a term value
    if (tok == TOK_CURRENT_GOAL) {
        parser_expect_consume(p, TOK_CURRENT_GOAL);
        return tactic_expr_current_goal();
    }

    // intro_step [name] - introduce a single forall-bound variable
    if (tok == TOK_INTRO_STEP) {
        parser_expect_consume(p, TOK_INTRO_STEP);
        AST *name = NULL;
        if (parser_expect_no_consume(p, TOK_IDENT)) {
            name = parse_atomic(p);
        }
        return tactic_expr_intro_step(name);
    }

    // match Goal with | [ ... |- ... ] => tac ... end
    // match <term>  with | <pat>        => tac ... end
    if (tok == TOK_MATCH) {
        parser_expect_consume(p, TOK_MATCH);

        if (parser_expect_no_consume(p, TOK_GOAL)) {
            parser_expect_consume(p, TOK_GOAL);
            if (!parser_expect_consume(p, TOK_WITH)) {
                parser_error(p, "Expected 'with' after 'match Goal'");
            }

            GoalBranch *branches = NULL;
            size_t branch_count = 0;

            while (parser_expect_no_consume(p, TOK_PIPE)) {
                parser_expect_consume(p, TOK_PIPE);
                if (!parser_expect_consume(p, TOK_LBRACKET)) {
                    parser_error(p, "Expected '[' after '|' in match goal branch");
                }

                // Parse hypothesis patterns: H : <pat> , H2 : <pat> , ... |- <concl>
                HypPattern *hyps = NULL;
                size_t hyp_count = 0;

                // If we see |- immediately, there are no hypothesis patterns
                if (!parser_expect_no_consume(p, TOK_TURNSTILE)) {
                    // Parse hypothesis patterns separated by commas
                    while (true) {
                        // Each hyp: <ident> : <term_pattern>
                        if (!parser_expect_no_consume(p, TOK_IDENT)) {
                            parser_error(p, "Expected hypothesis name in match goal pattern");
                        }
                        Token *hyp_name = parser_next(p);
                        if (!parser_expect_consume(p, TOK_COLON)) {
                            parser_error(p, "Expected ':' after hypothesis name");
                        }
                        AST *hyp_type = parse_term_pattern(p);

                        hyps = realloc(hyps, sizeof(HypPattern) * (hyp_count + 1));
                        hyps[hyp_count].name = strdup(hyp_name->lexeme);
                        hyps[hyp_count].type = hyp_type;
                        lexer_free_token(hyp_name);
                        hyp_count++;

                        if (!parser_expect_consume(p, TOK_COMMA)) {
                            break;
                        }
                    }
                }

                if (!parser_expect_consume(p, TOK_TURNSTILE)) {
                    parser_error(p, "Expected '|-' in match goal pattern");
                }

                AST *conclusion = parse_term_pattern(p);

                if (!parser_expect_consume(p, TOK_RBRACKET)) {
                    parser_error(p, "Expected ']' after match goal pattern");
                }
                if (!parser_expect_consume(p, TOK_DARROW)) {
                    parser_error(p, "Expected '=>' after match goal pattern");
                }

                TacticExpr *body = _parse_tactic_expr(p);

                branches = realloc(branches, sizeof(GoalBranch) * (branch_count + 1));
                branches[branch_count].hyps = hyps;
                branches[branch_count].hyp_count = hyp_count;
                branches[branch_count].conclusion = conclusion;
                branches[branch_count].body = body;
                branch_count++;
            }

            if (!parser_expect_consume(p, TOK_END)) {
                parser_error(p, "Expected 'end' after match goal branches");
            }

            return tactic_expr_match_goal(branches, branch_count);
        }

        // match <atomic_term> with | <term_pattern> => tac ... end
        AST *scrutinee = parse_atomic(p);
        if (!parser_expect_consume(p, TOK_WITH)) {
            parser_error(p, "Expected 'with' after match scrutinee");
        }

        TermBranch *term_branches = NULL;
        size_t term_branch_count = 0;

        while (parser_expect_no_consume(p, TOK_PIPE)) {
            parser_expect_consume(p, TOK_PIPE);
            AST *pattern = parse_term_pattern(p);
            if (!parser_expect_consume(p, TOK_DARROW)) {
                parser_error(p, "Expected '=>' after term pattern");
            }
            TacticExpr *body = _parse_tactic_expr(p);

            term_branches = realloc(term_branches, sizeof(TermBranch) * (term_branch_count + 1));
            term_branches[term_branch_count].pattern = pattern;
            term_branches[term_branch_count].body = body;
            term_branch_count++;
        }

        if (!parser_expect_consume(p, TOK_END)) {
            parser_error(p, "Expected 'end' after match term branches");
        }

        return tactic_expr_match_term(scrutinee, term_branches, term_branch_count);
    }

    // ( <tactic_expr> )
    if (tok == TOK_LPAREN) {
        parser_expect_consume(p, TOK_LPAREN);
        TacticExpr *inner = _parse_tactic_expr(p);
        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "Expected ')' in tactic expression");
        }
        return inner;
    }

    // pair <term> <term>
    if (tok == TOK_PAIR) {
        parser_expect_consume(p, TOK_PAIR);
        AST *fst = parse_atomic(p);
        AST *snd = parse_atomic(p);
        return tactic_expr_pair(fst, snd);
    }

    // fst <term>
    if (tok == TOK_FST) {
        parser_expect_consume(p, TOK_FST);
        AST *term = parse_atomic(p);
        return tactic_expr_fst(term);
    }

    // snd <term>
    if (tok == TOK_SND) {
        parser_expect_consume(p, TOK_SND);
        AST *term = parse_atomic(p);
        return tactic_expr_snd(term);
    }

    // app_func <term>
    if (tok == TOK_APP_FUNC) {
        parser_expect_consume(p, TOK_APP_FUNC);
        AST *term = parse_atomic(p);
        return tactic_expr_app_func(term);
    }

    // app_arg <term>
    if (tok == TOK_APP_ARG) {
        parser_expect_consume(p, TOK_APP_ARG);
        AST *term = parse_atomic(p);
        return tactic_expr_app_arg(term);
    }

    // expr_eq <term> <term>
    if (tok == TOK_EXPR_EQ) {
        parser_expect_consume(p, TOK_EXPR_EQ);
        AST *left = parse_atomic(p);
        AST *right = parse_atomic(p);
        return tactic_expr_expr_eq(left, right);
    }

    // rewrite_unify <lemma> <target>
    if (tok == TOK_REWRITE_UNIFY) {
        parser_expect_consume(p, TOK_REWRITE_UNIFY);
        AST *lemma = parse_atomic(p);
        AST *target = parse_atomic(p);
        return tactic_expr_rewrite_unify(lemma, target);
    }

    // constr <term> - evaluate a term expression and return as tactic value
    if (tok == TOK_CONSTR) {
        parser_expect_consume(p, TOK_CONSTR);
        AST *term = parse_atomic(p);
        return tactic_expr_constr(term);
    }

    // Primitive tactic
    InternalTacticParseFunc fn = _lookup_primitive(tok);
    if (fn) {
        Tactic *prim = fn(p);
        return tactic_expr_primitive(prim);
    }

    // User-defined tactic call: <ident> { <term_arg> }
    if (tok == TOK_IDENT) {
        Token *name_tok = parser_next(p);
        char *name = strdup(name_tok->lexeme);
        lexer_free_token(name_tok);

        // Parse atomic term arguments until a combinator/separator token
        AST **args = NULL;
        size_t arg_count = 0;

        while (!parser_eof(p)) {
            TokenType next = p->current->type;
            // Stop at combinator tokens, separators, and terminators
            if (next == TOK_SEMICOLON || next == TOK_DOUBLE_PIPE || next == TOK_DOT ||
                next == TOK_RPAREN || next == TOK_RBRACKET || next == TOK_PIPE ||
                next == TOK_END || next == TOK_IN) {
                break;
            }
            AST *arg = parse_atomic(p);
            args = realloc(args, sizeof(AST *) * (arg_count + 1));
            args[arg_count++] = arg;
        }

        return tactic_expr_call(name, args, arg_count);
    }

    parser_error(p, "Unknown or unsupported tactic keyword");
    return NULL;  // unreachable
}

/* ============================================================================
 * Public API
 * ============================================================================ */

TacticExpr *tactic_parse_proof_command(Parser *p) {
    // Handle "Admitted." specially - it's not composable
    if (parser_expect_no_consume(p, TOK_ADMITTED)) {
        parser_expect_consume(p, TOK_ADMITTED);
        if (!parser_expect_consume(p, TOK_DOT)) {
            parser_error(p, "Expected '.' at end of Admitted");
        }
        Tactic *tactic = malloc(sizeof(Tactic));
        tactic->tag = TACTIC_ADMITTED;
        return tactic_expr_primitive(tactic);
    }

    TacticExpr *expr = _parse_tactic_expr(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of tactic");
    }

    return expr;
}

TacticExpr *tactic_parse_expr(Parser *p) { return _parse_tactic_expr(p); }
