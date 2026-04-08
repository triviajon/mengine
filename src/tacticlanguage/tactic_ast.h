#ifndef TACTIC_AST_H
#define TACTIC_AST_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "src/tacticlanguage/tactic_parser.h"

typedef enum {
    TAC_PRIMITIVE,  // wraps an existing Tactic*
    TAC_SEQ,        // tac1 ; tac2
    TAC_ORELSE,     // tac1 || tac2
    TAC_TRY,        // try tac
    TAC_REPEAT,     // repeat tac
    TAC_FIRST,      // first [ tac1 | tac2 | ... ]
    TAC_IDTAC,      // idtac (identity, always succeeds)
    TAC_FAIL,       // fail (always fails)
    TAC_CALL,        // user-defined tactic call
    TAC_MATCH_GOAL,  // match goal with | [ ... |- ... ] => tac end
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
    HypPattern *hyps;    // array of hypothesis patterns
    size_t hyp_count;    // number of hypothesis patterns
    AST *conclusion;     // conclusion pattern (after |-)
    TacticExpr *body;    // tactic body to execute on match
} GoalBranch;

typedef struct {
    GoalBranch *branches;  // array of goal branches
    size_t branch_count;   // number of branches
} MatchGoalTacticExpr;

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
    // Replace existing definition with same name
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->defs[i]->name, def->name) == 0) {
            // TODO: free old def
            env->defs[i] = def;
            return;
        }
    }
    if (env->count >= env->capacity) {
        env->capacity = env->capacity == 0 ? 8 : env->capacity * 2;
        env->defs = realloc(env->defs, sizeof(TacticDef *) * env->capacity);
    }
    env->defs[env->count++] = def;
}

static inline TacticDef *tactic_env_lookup(TacticEnv *env, const char *name) {
    if (!env) return NULL;
    for (size_t i = 0; i < env->count; i++) {
        if (strcmp(env->defs[i]->name, name) == 0) {
            return env->defs[i];
        }
    }
    return NULL;
}

static inline void tactic_env_free(TacticEnv *env) {
    if (!env) return;
    // TODO: deep-free definitions
    free(env->defs);
    free(env);
}

#endif  // TACTIC_AST_H
