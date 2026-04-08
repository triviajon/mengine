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
    TAC_CALL,       // user-defined tactic call
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
    char *name;         // tactic name
    AST **args;         // term arguments (NULL if no args)
    size_t arg_count;   // number of arguments
} CallTacticExpr;

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

#endif  // TACTIC_AST_H
