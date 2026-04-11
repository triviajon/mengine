#include "src/commandlanguage/command_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/common/lexer.h"
#include "src/common/parser_base.h"
#include "src/tacticlanguage/tactic_ast.h"
#include "src/termlanguage/ast_to_expression.h"
#include "src/termlanguage/parser.h"

char *decl_keyword_to_string(DeclKeyword kw) {
    switch (kw) {
        case DECL_KW_AXIOM:
            return "Axiom";
        case DECL_KW_VARIABLE:
            return "Variable";
        default:
            return "UnknownDeclKeyword";
    }
}

char *stmt_keyword_to_string(StmtKeyword kw) {
    switch (kw) {
        case STMT_KW_THEOREM:
            return "Theorem";
        case STMT_KW_LEMMA:
            return "Lemma";
        default:
            return "UnknownStmtKeyword";
    }
}

Command *command_parse_command(Parser *p) {
    TokenType tok_type = p->current->type;

    CommandDispatchEntry *entry = NULL;
    CMD_LOOKUP_ENTRY(&entry, tok_type);

    if (entry == NULL) {
        parser_error(p, "Unknown or unsupported command keyword");
    }

    CommandParseFunc fn = CMD_ENTRY_PARSER(entry);
    if (!fn) {
        parser_error(p, "Internal error: null parse function");
    }

    Command *cmd = fn(p);
    return cmd;
}

Command *command_parse_declaration(Parser *p) {
    DeclKeyword kw = command_parse_declaration_keyword(p);
    Binder *binder = command_parse_assumption(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of declaration");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DECLARATION;
    cmd->as.decl.binder = *binder;
    cmd->as.decl.kw = kw;

    free(binder);

    return cmd;
}

DeclKeyword command_parse_declaration_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_AXIOM)) {
        return DECL_KW_AXIOM;
    }
    if (parser_expect_consume(p, TOK_VARIABLE)) {
        return DECL_KW_VARIABLE;
    }
    parser_error(p, "expected 'Axiom' or 'Variable' keyword");

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return DECL_KW_VARIABLE;
}

Binder *command_parse_assumption(Parser *p) { return parse_binder(p); }

/**
 * <constructor> ::= "|" <identifier> ":" <term>
 *
 * @param p Pointer to the Parser.
 * @return InductiveConstructor structure representing the parsed constructor.
 */
InductiveConstructor *command_parse_constructor(Parser *p) {
    if (!parser_expect_consume(p, TOK_PIPE)) {
        parser_error(p, "expected '|' before constructor");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected constructor name after '|'");
    }

    Token *ctor_token = parser_next(p);
    char *ctor_name = strdup(ctor_token->lexeme);
    lexer_free_token(ctor_token);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' after constructor name");
    }

    AST *ctor_type = parse_term(p);
    debug_print_ast(p, ctor_type);

    InductiveConstructor *ctor = malloc(sizeof(InductiveConstructor));
    ctor->name = ctor_name;
    ctor->type = ctor_type;

    return ctor;
}

Command *command_parse_definition(Parser *p) {
    if (!parser_expect_consume(p, TOK_DEFINITION)) {
        parser_error(p, "expected 'Definition'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Definition'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    Binder **params = NULL;
    size_t param_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *b = command_parse_assumption(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "expected ')' after parameter");
        }

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before type");
    }

    AST *type = parse_term(p);
    debug_print_ast(p, type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' in definition");
    }

    AST *body = parse_term(p);
    debug_print_ast(p, body);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_DEFINITION;
    cmd->as.defn.name = name;
    cmd->as.defn.params = params;
    cmd->as.defn.param_count = param_count;
    cmd->as.defn.type = type;
    cmd->as.defn.body = body;

    return cmd;
}

Command *command_parse_statement(Parser *p) {
    StmtKeyword kw = command_parse_statement_keyword(p);

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after theorem keyword");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before theorem type");
    }

    AST *ty = parse_term(p);
    debug_print_ast(p, ty);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after theorem statement");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_STATEMENT;
    cmd->as.stmt.kw = kw;
    cmd->as.stmt.name = name;
    cmd->as.stmt.type = ty;

    return cmd;
}

StmtKeyword command_parse_statement_keyword(Parser *p) {
    if (parser_expect_consume(p, TOK_THEOREM)) {
        return STMT_KW_THEOREM;
    }
    if (parser_expect_consume(p, TOK_LEMMA)) {
        return STMT_KW_LEMMA;
    }
    parser_error(p, "expected 'Theorem' or 'Lemma' keyword");

    // Unreachable, but avoids compiler warning.
    // TODO: refactor error handling
    return STMT_KW_LEMMA;
}

Command *command_parse_fixpoint(Parser *p) {
    if (!parser_expect_consume(p, TOK_FIXPOINT)) {
        parser_error(p, "expected 'Fixpoint'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Fixpoint'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    Binder **binders = NULL;
    size_t binder_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *b = parse_binder(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "expected ')' after binder");
        }

        binders = realloc(binders, sizeof(Binder *) * (binder_count + 1));
        if (!binders) {
            parser_error(p, "Memory allocation failed for binders");
        }
        binders[binder_count] = b;
        binder_count++;
    }

    char *decreasing_arg_name = parse_decreasing_arg_annotation(p);

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' after decreasing arg annotation");
    }

    AST *return_type = parse_term(p);
    debug_print_ast(p, return_type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' after return type");
    }

    AST *body = parse_term(p);
    debug_print_ast(p, body);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after fixpoint definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_FIXPOINT;
    cmd->as.fixpoint.name = name;
    cmd->as.fixpoint.binders = binders;
    cmd->as.fixpoint.binder_count = binder_count;
    cmd->as.fixpoint.decreasing_arg_name = decreasing_arg_name;
    cmd->as.fixpoint.return_type = return_type;
    cmd->as.fixpoint.body = body;
    return cmd;
}

Command *command_parse_check(Parser *p) {
    if (!parser_expect_consume(p, TOK_CHECK)) {
        parser_error(p, "expected 'Check'");
    }

    AST *term = parse_term(p);
    debug_print_ast(p, term);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of check command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_CHECK;
    cmd->as.check.term = term;
    return cmd;
}

Command *command_parse_print(Parser *p) {
    if (!parser_expect_consume(p, TOK_PRINT)) {
        parser_error(p, "expected 'Print'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Print'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "Expected '.' at end of print command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_PRINT;
    cmd->as.print.name = name;
    return cmd;
}

Command *command_parse_show(Parser *p) {
    if (!parser_expect_consume(p, TOK_SHOW)) {
        parser_error(p, "expected 'Show'");
    }

    ShowKeyword sh_kw = command_parse_show_type(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after show command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_SHOW;
    cmd->as.show.kw = sh_kw;

    return cmd;
}

ShowKeyword command_parse_show_type(Parser *p) {
    if (parser_expect_consume(p, TOK_CONTEXT)) {
        return SHOW_KW_CONTEXT;
    }

    if (parser_expect_consume(p, TOK_PROOF)) {
        return SHOW_KW_PROOF;
    }

    if (parser_expect_consume(p, TOK_GOAL)) {
        return SHOW_KW_GOAL;
    }

    if (parser_expect_consume(p, TOK_STATE)) {
        return SHOW_KW_STATE;
    }

    parser_error(p, "expected 'Context', 'Proof', 'Goal', or 'State' after 'Show'");

    // Unreachable, but avoids compiler warning.
    return SHOW_KW_STATE;
}

Command *command_parse_inductive(Parser *p) {
    if (!parser_expect_consume(p, TOK_INDUCTIVE)) {
        parser_error(p, "expected 'Inductive'");
    }

    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected identifier after 'Inductive'");
    }

    Token *ident_token = parser_next(p);
    char *name = strdup(ident_token->lexeme);
    lexer_free_token(ident_token);

    // Binders
    Binder **params = NULL;
    size_t param_count = 0;

    while (parser_expect_consume(p, TOK_LPAREN)) {
        Binder *b = parse_binder(p);

        if (!parser_expect_consume(p, TOK_RPAREN)) {
            parser_error(p, "expected ')' after parameter");
        }

        params = realloc(params, sizeof(Binder *) * (param_count + 1));
        params[param_count] = b;
        param_count++;
    }

    if (!parser_expect_consume(p, TOK_COLON)) {
        parser_error(p, "expected ':' before inductive type");
    }

    // Type
    AST *type = parse_term(p);
    debug_print_ast(p, type);

    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' after inductive type");
    }

    // Constructors
    InductiveConstructor **constructors = NULL;
    size_t constructor_count = 0;

    while (parser_expect_no_consume(p, TOK_PIPE)) {
        InductiveConstructor *ctor = command_parse_constructor(p);

        constructors =
            realloc(constructors, sizeof(InductiveConstructor *) * (constructor_count + 1));
        constructors[constructor_count] = ctor;
        constructor_count++;
    }

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' after inductive definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_INDUCTIVE;
    cmd->as.inductive.name = name;
    cmd->as.inductive.params = params;
    cmd->as.inductive.param_count = param_count;
    cmd->as.inductive.type = type;
    cmd->as.inductive.constructors = constructors;
    cmd->as.inductive.constructor_count = constructor_count;

    return cmd;
}

Command *command_parse_eval(Parser *p) {
    if (!parser_expect_consume(p, TOK_EVAL)) {
        parser_error(p, "expected 'Eval'");
    }

    // Parse strategy
    EvalStrategy strategy = 0;
    if (parser_expect_consume(p, TOK_CBV)) {
        strategy = EVAL_STRATEGY_CBV;
    } else if (parser_expect_consume(p, TOK_COMPUTE)) {
        strategy = EVAL_STRATEGY_COMPUTE;
    } else {
        parser_error(p, "expected reduction strategy: 'cbv', 'compute', or 'hnf'");
    }

    // Parse optional flags
    bool beta_flag = false;
    bool delta_flag = false;
    bool iota_flag = false;
    bool fix_flag = false;

    // For cbv, we can optionally specify which reductions to apply
    if (strategy == EVAL_STRATEGY_CBV) {
        bool found_flag = true;
        while (found_flag) {
            if (parser_expect_consume(p, TOK_BETA)) {
                beta_flag = true;
            } else if (parser_expect_consume(p, TOK_DELTA)) {
                delta_flag = true;
            } else if (parser_expect_consume(p, TOK_IOTA)) {
                iota_flag = true;
            } else if (parser_expect_consume(p, TOK_FIX)) {
                fix_flag = true;
            } else {
                found_flag = false;
            }
        }
    }

    // Expect 'in' keyword
    if (!parser_expect_consume(p, TOK_IN)) {
        parser_error(p, "expected 'in' after reduction strategy");
    }

    // Parse the term to evaluate
    AST *term = parse_term(p);
    debug_print_ast(p, term);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' at end of eval command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_EVAL;
    cmd->as.eval.strategy = strategy;
    cmd->as.eval.beta_flag = beta_flag;
    cmd->as.eval.delta_flag = delta_flag;
    cmd->as.eval.iota_flag = iota_flag;
    cmd->as.eval.fix_flag = fix_flag;
    cmd->as.eval.term = term;

    return cmd;
}

Command *command_parse_tactic_def(Parser *p) {
    // Tactic <name> { <param> } := <tactic_expr> .
    if (!parser_expect_consume(p, TOK_TACTIC)) {
        parser_error(p, "expected 'Tactic'");
    }

    // Parse tactic name
    if (!parser_expect_no_consume(p, TOK_IDENT)) {
        parser_error(p, "expected tactic name after 'Tactic'");
    }
    Token *name_tok = parser_next(p);
    char *name = strdup(name_tok->lexeme);
    lexer_free_token(name_tok);

    // Parse parameter names (identifiers before ':=')
    char **params = NULL;
    size_t param_count = 0;

    while (parser_expect_no_consume(p, TOK_IDENT)) {
        Token *param_tok = parser_next(p);
        params = realloc(params, sizeof(char *) * (param_count + 1));
        params[param_count++] = strdup(param_tok->lexeme);
        lexer_free_token(param_tok);
    }

    // Expect ':='
    if (!parser_expect_consume(p, TOK_COLON_EQ)) {
        parser_error(p, "expected ':=' in Tactic definition");
    }

    // Parse body as a tactic expression (no trailing '.')
    TacticExpr *body = tactic_parse_expr(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' at end of Tactic definition");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_TACTIC_DEF;
    cmd->as.tactic_def.name = name;
    cmd->as.tactic_def.params = params;
    cmd->as.tactic_def.param_count = param_count;
    cmd->as.tactic_def.body = body;

    return cmd;
}

Command *command_parse_register_relation(Parser *p) {
    // Register Relation <relation> <refl> <trans> <congr> .
    if (!parser_expect_consume(p, TOK_REGISTER)) {
        parser_error(p, "expected 'Register'");
    }
    if (!parser_expect_consume(p, TOK_RELATION)) {
        parser_error(p, "expected 'Relation' after 'Register'");
    }

    AST *relation = parse_atomic(p);
    AST *refl = parse_atomic(p);
    AST *trans = parse_atomic(p);
    AST *congr = parse_atomic(p);

    if (!parser_expect_consume(p, TOK_DOT)) {
        parser_error(p, "expected '.' at end of Register Relation command");
    }

    Command *cmd = malloc(sizeof(Command));
    cmd->tag = CMD_REGISTER_RELATION;
    cmd->as.register_relation.relation = relation;
    cmd->as.register_relation.refl = refl;
    cmd->as.register_relation.trans = trans;
    cmd->as.register_relation.congr = congr;

    return cmd;
}

void free_command(Command *cmd) {
    if (!cmd) return;

    switch (cmd->tag) {
        case CMD_SHOW:
            break;
        case CMD_CHECK:
            free_ast(cmd->as.check.term);
            break;
        case CMD_PRINT:
            free(cmd->as.print.name);
            break;
        case CMD_EVAL:
            free_ast(cmd->as.eval.term);
            break;
        case CMD_DECLARATION:
            free(cmd->as.decl.binder.name);
            free_ast(cmd->as.decl.binder.type);
            break;
        case CMD_DEFINITION:
            free(cmd->as.defn.name);
            for (size_t i = 0; i < cmd->as.defn.param_count; i++) {
                free(cmd->as.defn.params[i]->name);
                free_ast(cmd->as.defn.params[i]->type);
                free(cmd->as.defn.params[i]);
            }
            free(cmd->as.defn.params);
            free_ast(cmd->as.defn.type);
            free_ast(cmd->as.defn.body);
            break;
        case CMD_STATEMENT:
            free(cmd->as.stmt.name);
            free_ast(cmd->as.stmt.type);
            break;
        case CMD_INDUCTIVE: {
            free(cmd->as.inductive.name);
            for (size_t i = 0; i < cmd->as.inductive.param_count; i++) {
                Binder *param = cmd->as.inductive.params[i];
                free(param->name);
                free_ast(param->type);
                free(param);
            }
            free(cmd->as.inductive.params);
            free_ast(cmd->as.inductive.type);
            for (size_t i = 0; i < cmd->as.inductive.constructor_count; i++) {
                InductiveConstructor *ctor = cmd->as.inductive.constructors[i];
                free(ctor->name);
                free_ast(ctor->type);
                free(ctor);
            }
            free(cmd->as.inductive.constructors);
            break;
        }
        case CMD_FIXPOINT: {
            free(cmd->as.fixpoint.name);
            for (size_t i = 0; i < cmd->as.fixpoint.binder_count; i++) {
                Binder *binder = cmd->as.fixpoint.binders[i];
                free(binder->name);
                free_ast(binder->type);
                free(binder);
            }
            free(cmd->as.fixpoint.binders);
            free(cmd->as.fixpoint.decreasing_arg_name);
            free_ast(cmd->as.fixpoint.return_type);
            free_ast(cmd->as.fixpoint.body);
            break;
        }
        case CMD_TACTIC_DEF:
            // Ownership of name, params, and body is transferred to
            // the tactic environment by _handle_tactic_def_command.
            break;
        case CMD_REGISTER_RELATION:
            free_ast(cmd->as.register_relation.relation);
            free_ast(cmd->as.register_relation.refl);
            free_ast(cmd->as.register_relation.trans);
            free_ast(cmd->as.register_relation.congr);
            break;
    }

    free(cmd);
}