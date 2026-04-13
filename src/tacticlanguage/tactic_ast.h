#ifndef TACTIC_AST_H
#define TACTIC_AST_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "src/tacticlanguage/tactic_parser.h"

typedef enum {
    TAC_PRIMITIVE,      // wraps an existing Tactic*
    TAC_SEQ,            // tac1 ; tac2
    TAC_ORELSE,         // tac1 || tac2
    TAC_TRY,            // try tac
    TAC_REPEAT,         // repeat tac
    TAC_FIRST,          // first [ tac1 | tac2 | ... ]
    TAC_IDTAC,          // idtac (identity, always succeeds)
    TAC_FAIL,           // fail (always fails)
    TAC_CALL,           // user-defined tactic call
    TAC_MATCH_GOAL,     // match goal with | [ ... |- ... ] => tac end
    TAC_LET,            // let x := <tactic_expr> in <tactic_expr>
    TAC_GOAL_TYPE,      // goal_type - returns the type of the current goal
    TAC_TYPE_OF,        // type_of <term> - returns the type of a term
    TAC_MATCH_TERM,     // match <term> with | <pat> => tac ... end
    TAC_MK_HOLE,        // mk_hole <type>        - creates a hole, returns it as term_value
    TAC_FILL,           // fill <hole> <term>    - fills hole with term
    TAC_SUBST,          // subst <new> <body> <old> - substitution body[old := new]
    TAC_EUNIFY,         // eunify <lemma> <goal> - existential unification
    TAC_CURRENT_GOAL,   // current_goal - returns the current goal hole as a term value
    TAC_INTRO_STEP,     // intro_step [name] - introduce a forall-bound variable
    TAC_PAIR,           // pair <term> <term> - create a tactic-level pair
    TAC_FST,            // fst <term> - extract first element of a pair
    TAC_SND,            // snd <term> - extract second element of a pair
    TAC_APP_FUNC,       // app_func <term> - get function part of application
    TAC_APP_ARG,        // app_arg <term> - get argument part of application
    TAC_EXPR_EQ,        // expr_eq <term> <term> - pointer equality test
    TAC_REWRITE_UNIFY,  // rewrite_unify <lemma> <target> - rewrite head unification
    TAC_CONSTR,         // constr <term> - evaluate a term and return it as a value
} TacticExprTag;

typedef struct TacticExpr TacticExpr;

typedef struct {
    Tactic *tactic;
} PrimitiveTacticExpr;

typedef struct {
    TacticExpr *left;
    TacticExpr *right;
} SeqTacticExpr;

typedef struct {
    TacticExpr *left;
    TacticExpr *right;
} OrelseTacticExpr;

typedef struct {
    TacticExpr *body;
} TryTacticExpr;

typedef struct {
    TacticExpr *body;
} RepeatTacticExpr;

typedef struct {
    TacticExpr **alternatives;
    size_t count;
} FirstTacticExpr;

typedef struct {
    char *name;        // tactic name
    AST **args;        // term arguments (NULL if no args)
    size_t arg_count;  // number of arguments
} CallTacticExpr;

typedef struct {
    char *name;  // hypothesis binding name (e.g. "H" in "H : ?P")
    AST *type;   // type pattern (AST with AST_PATVAR nodes)
} HypPattern;

typedef struct {
    HypPattern *hyps;  // array of hypothesis patterns
    size_t hyp_count;  // number of hypothesis patterns
    AST *conclusion;   // conclusion pattern (after |-)
    TacticExpr *body;  // tactic body to execute on match
} GoalBranch;

typedef struct {
    GoalBranch *branches;  // array of goal branches
    size_t branch_count;   // number of branches
} MatchGoalTacticExpr;

typedef struct {
    char *name;        // binding name
    TacticExpr *rhs;   // right-hand side (value-producing expression)
    TacticExpr *body;  // body in which name is bound
} LetTacticExpr;

typedef struct {
    AST *term;  // term whose type to compute
} TypeOfTacticExpr;

typedef struct {
    AST *pattern;      // term pattern (with AST_PATVAR nodes)
    TacticExpr *body;  // tactic body to execute on match
} TermBranch;

typedef struct {
    AST *scrutinee;  // term to match (AST evaluated at interpret-time)
    TermBranch *branches;
    size_t branch_count;
} MatchTermTacticExpr;

typedef struct {
    AST *type;  // type of the hole to create
} MkHoleTacticExpr;

typedef struct {
    AST *hole;  // hole expression
    AST *term;  // term to fill with
} FillTacticExpr;

typedef struct {
    AST *new_term;  // replacement
    AST *body;      // expression to substitute into
    AST *old_var;   // variable to replace
} SubstTacticExpr;

typedef struct {
    AST *lemma;  // lemma to unify against current goal
} EunifyTacticExpr;

typedef struct {
    AST *name;  // optional name for the variable (NULL = use forall's name)
} IntroStepTacticExpr;

typedef struct {
    AST *fst;  // first element
    AST *snd;  // second element
} PairTacticExpr;

typedef struct {
    AST *term;  // pair to destructure
} FstTacticExpr;

typedef struct {
    AST *term;  // pair to destructure
} SndTacticExpr;

typedef struct {
    AST *term;  // application expression to inspect
} AppFuncTacticExpr;

typedef struct {
    AST *term;  // application expression to inspect
} AppArgTacticExpr;

typedef struct {
    AST *left;   // first expression
    AST *right;  // second expression
} ExprEqTacticExpr;

typedef struct {
    AST *lemma;   // lemma to unify
    AST *target;  // target expression
} RewriteUnifyTacticExpr;

typedef struct {
    AST *term;  // term to evaluate
} ConstrTacticExpr;

struct TacticExpr {
    TacticExprTag tag;
    union {
        PrimitiveTacticExpr primitive;
        SeqTacticExpr seq;
        OrelseTacticExpr orelse;
        TryTacticExpr try_expr;
        RepeatTacticExpr repeat;
        FirstTacticExpr first;
        CallTacticExpr call;
        MatchGoalTacticExpr match_goal;
        LetTacticExpr let_expr;
        TypeOfTacticExpr type_of;
        MatchTermTacticExpr match_term;
        MkHoleTacticExpr mk_hole;
        FillTacticExpr fill;
        SubstTacticExpr subst;
        EunifyTacticExpr eunify;
        IntroStepTacticExpr intro_step;
        PairTacticExpr pair;
        FstTacticExpr fst;
        SndTacticExpr snd;
        AppFuncTacticExpr app_func;
        AppArgTacticExpr app_arg;
        ExprEqTacticExpr expr_eq;
        RewriteUnifyTacticExpr rewrite_unify;
        ConstrTacticExpr constr;
    } as;
};

static inline TacticExpr *tactic_expr_primitive(Tactic *tactic) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_PRIMITIVE;
    e->as.primitive.tactic = tactic;
    return e;
}

static inline TacticExpr *tactic_expr_seq(TacticExpr *left, TacticExpr *right) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_SEQ;
    e->as.seq.left = left;
    e->as.seq.right = right;
    return e;
}

static inline TacticExpr *tactic_expr_orelse(TacticExpr *left, TacticExpr *right) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_ORELSE;
    e->as.orelse.left = left;
    e->as.orelse.right = right;
    return e;
}

static inline TacticExpr *tactic_expr_try(TacticExpr *body) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_TRY;
    e->as.try_expr.body = body;
    return e;
}

static inline TacticExpr *tactic_expr_repeat(TacticExpr *body) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_REPEAT;
    e->as.repeat.body = body;
    return e;
}

static inline TacticExpr *tactic_expr_first(TacticExpr **alternatives, size_t count) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_FIRST;
    e->as.first.alternatives = alternatives;
    e->as.first.count = count;
    return e;
}

static inline TacticExpr *tactic_expr_idtac(void) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_IDTAC;
    return e;
}

static inline TacticExpr *tactic_expr_fail(void) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_FAIL;
    return e;
}

static inline TacticExpr *tactic_expr_call(char *name, AST **args, size_t arg_count) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_CALL;
    e->as.call.name = strdup(name);
    e->as.call.args = args;
    e->as.call.arg_count = arg_count;
    return e;
}

static inline TacticExpr *tactic_expr_match_goal(GoalBranch *branches, size_t branch_count) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_MATCH_GOAL;
    e->as.match_goal.branches = branches;
    e->as.match_goal.branch_count = branch_count;
    return e;
}

static inline TacticExpr *tactic_expr_let(char *name, TacticExpr *rhs, TacticExpr *body) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_LET;
    e->as.let_expr.name = strdup(name);
    e->as.let_expr.rhs = rhs;
    e->as.let_expr.body = body;
    return e;
}

static inline TacticExpr *tactic_expr_goal_type(void) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_GOAL_TYPE;
    return e;
}

static inline TacticExpr *tactic_expr_type_of(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_TYPE_OF;
    e->as.type_of.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_match_term(AST *scrutinee, TermBranch *branches,
                                                 size_t branch_count) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_MATCH_TERM;
    e->as.match_term.scrutinee = scrutinee;
    e->as.match_term.branches = branches;
    e->as.match_term.branch_count = branch_count;
    return e;
}

static inline TacticExpr *tactic_expr_mk_hole(AST *type) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_MK_HOLE;
    e->as.mk_hole.type = type;
    return e;
}

static inline TacticExpr *tactic_expr_fill(AST *hole, AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_FILL;
    e->as.fill.hole = hole;
    e->as.fill.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_subst(AST *new_term, AST *body, AST *old_var) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_SUBST;
    e->as.subst.new_term = new_term;
    e->as.subst.body = body;
    e->as.subst.old_var = old_var;
    return e;
}

static inline TacticExpr *tactic_expr_eunify(AST *lemma) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_EUNIFY;
    e->as.eunify.lemma = lemma;
    return e;
}

static inline TacticExpr *tactic_expr_current_goal(void) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_CURRENT_GOAL;
    return e;
}

static inline TacticExpr *tactic_expr_intro_step(AST *name) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_INTRO_STEP;
    e->as.intro_step.name = name;
    return e;
}

static inline TacticExpr *tactic_expr_pair(AST *fst, AST *snd) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_PAIR;
    e->as.pair.fst = fst;
    e->as.pair.snd = snd;
    return e;
}

static inline TacticExpr *tactic_expr_fst(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_FST;
    e->as.fst.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_snd(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_SND;
    e->as.snd.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_app_func(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_APP_FUNC;
    e->as.app_func.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_app_arg(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_APP_ARG;
    e->as.app_arg.term = term;
    return e;
}

static inline TacticExpr *tactic_expr_expr_eq(AST *left, AST *right) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_EXPR_EQ;
    e->as.expr_eq.left = left;
    e->as.expr_eq.right = right;
    return e;
}

static inline TacticExpr *tactic_expr_rewrite_unify(AST *lemma, AST *target) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_REWRITE_UNIFY;
    e->as.rewrite_unify.lemma = lemma;
    e->as.rewrite_unify.target = target;
    return e;
}

static inline TacticExpr *tactic_expr_constr(AST *term) {
    TacticExpr *e = malloc(sizeof(TacticExpr));
    e->tag = TAC_CONSTR;
    e->as.constr.term = term;
    return e;
}

/* --------------------------------------------------------------------------
 * free_tactic_expr – recursively free a TacticExpr tree
 * -------------------------------------------------------------------------- */

void free_ast(AST *ast);        // forward declaration (defined in ast_to_expression.c)
// void free_tactic(Tactic *tac);  // forward declaration (defined in tactic_parser.c)

static inline void free_tactic_expr(TacticExpr *expr) {
    if (!expr) { return; }
    switch (expr->tag) {
        case TAC_PRIMITIVE:
            free_tactic(expr->as.primitive.tactic);
            break;
        case TAC_SEQ:
            free_tactic_expr(expr->as.seq.left);
            free_tactic_expr(expr->as.seq.right);
            break;
        case TAC_ORELSE:
            free_tactic_expr(expr->as.orelse.left);
            free_tactic_expr(expr->as.orelse.right);
            break;
        case TAC_TRY:
            free_tactic_expr(expr->as.try_expr.body);
            break;
        case TAC_REPEAT:
            free_tactic_expr(expr->as.repeat.body);
            break;
        case TAC_FIRST:
            for (size_t i = 0; i < expr->as.first.count; i++) {
                free_tactic_expr(expr->as.first.alternatives[i]);
            }
            free(expr->as.first.alternatives);
            break;
        case TAC_IDTAC:
        case TAC_FAIL:
        case TAC_GOAL_TYPE:
        case TAC_CURRENT_GOAL:
            break;
        case TAC_CALL:
            for (size_t i = 0; i < expr->as.call.arg_count; i++) {
                free_ast(expr->as.call.args[i]);
            }
            free(expr->as.call.args);
            free(expr->as.call.name);
            break;
        case TAC_MATCH_GOAL:
            for (size_t i = 0; i < expr->as.match_goal.branch_count; i++) {
                GoalBranch *b = &expr->as.match_goal.branches[i];
                for (size_t j = 0; j < b->hyp_count; j++) {
                    free(b->hyps[j].name);
                    free_ast(b->hyps[j].type);
                }
                free(b->hyps);
                free_ast(b->conclusion);
                free_tactic_expr(b->body);
            }
            free(expr->as.match_goal.branches);
            break;
        case TAC_LET:
            free(expr->as.let_expr.name);
            free_tactic_expr(expr->as.let_expr.rhs);
            free_tactic_expr(expr->as.let_expr.body);
            break;
        case TAC_TYPE_OF:
            free_ast(expr->as.type_of.term);
            break;
        case TAC_MATCH_TERM:
            free_ast(expr->as.match_term.scrutinee);
            for (size_t i = 0; i < expr->as.match_term.branch_count; i++) {
                free_ast(expr->as.match_term.branches[i].pattern);
                free_tactic_expr(expr->as.match_term.branches[i].body);
            }
            free(expr->as.match_term.branches);
            break;
        case TAC_MK_HOLE:
            free_ast(expr->as.mk_hole.type);
            break;
        case TAC_FILL:
            free_ast(expr->as.fill.hole);
            free_ast(expr->as.fill.term);
            break;
        case TAC_SUBST:
            free_ast(expr->as.subst.new_term);
            free_ast(expr->as.subst.body);
            free_ast(expr->as.subst.old_var);
            break;
        case TAC_EUNIFY:
            free_ast(expr->as.eunify.lemma);
            break;
        case TAC_INTRO_STEP:
            if (expr->as.intro_step.name) { free_ast(expr->as.intro_step.name); }
            break;
        case TAC_PAIR:
            free_ast(expr->as.pair.fst);
            free_ast(expr->as.pair.snd);
            break;
        case TAC_FST:
            free_ast(expr->as.fst.term);
            break;
        case TAC_SND:
            free_ast(expr->as.snd.term);
            break;
        case TAC_APP_FUNC:
            free_ast(expr->as.app_func.term);
            break;
        case TAC_APP_ARG:
            free_ast(expr->as.app_arg.term);
            break;
        case TAC_EXPR_EQ:
            free_ast(expr->as.expr_eq.left);
            free_ast(expr->as.expr_eq.right);
            break;
        case TAC_REWRITE_UNIFY:
            free_ast(expr->as.rewrite_unify.lemma);
            free_ast(expr->as.rewrite_unify.target);
            break;
        case TAC_CONSTR:
            free_ast(expr->as.constr.term);
            break;
    }
    free(expr);
}

/* ============================================================================
 * Tactic definitions (stored in the tactic environment)
 * ============================================================================ */

typedef struct {
    char *name;          // tactic name
    char **params;       // parameter names (NULL if none)
    size_t param_count;  // number of parameters
    TacticExpr *body;    // body expression
} TacticDef;

typedef struct TacticEnv {
    TacticDef **defs;  // dynamic array of definitions
    size_t count;      // number of definitions
    size_t capacity;   // allocated capacity
} TacticEnv;

static inline TacticEnv *tactic_env_new(void) {
    TacticEnv *env = malloc(sizeof(TacticEnv));
    env->defs = NULL;
    env->count = 0;
    env->capacity = 0;
    return env;
}

static inline void tactic_env_add(TacticEnv *env, TacticDef *def) {
    // Replace existing definition with same name AND same arity
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->defs[i]->name, def->name) == 0 &&
            env->defs[i]->param_count == def->param_count) {
            TacticDef *old = env->defs[i];
            env->defs[i] = def;
            free(old->name);
            if (old->params) {
                for (size_t j = 0; j < old->param_count; j++) {
                    free(old->params[j]);
                }
                free(old->params);
            }
            free_tactic_expr(old->body);
            free(old);
            return;
        }
    }
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
        env->defs = realloc(env->defs, sizeof(TacticDef *) * env->capacity);
    }
    env->defs[env->count++] = def;
}

static inline TacticDef *tactic_env_lookup(TacticEnv *env, const char *name, size_t arg_count) {
    if (!env) {
        return NULL;
    }
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->defs[i]->name, name) == 0 && env->defs[i]->param_count == arg_count) {
            return env->defs[i];
        }
    }
    return NULL;
}

static inline void tactic_env_free(TacticEnv *env) {
    if (!env) {
        return;
    }
    for (size_t i = 0; i < env->count; i++) {
        TacticDef *def = env->defs[i];
        if (def) {
            free(def->name);
            if (def->params) {
                for (size_t j = 0; j < def->param_count; j++) {
                    free(def->params[j]);
                }
                free(def->params);
            }
            free_tactic_expr(def->body);
            free(def);
        }
    }
    free(env->defs);
    free(env);
}

#endif  // TACTIC_AST_H
