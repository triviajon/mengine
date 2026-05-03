#ifndef TACTIC_API_H
#define TACTIC_API_H

#include <stdbool.h>
#include <stdlib.h>

#include "src/common/doubly_linked_list.h"

typedef struct Expression Expression;
typedef struct TacticResult TacticResult;
typedef struct TacticValue TacticValue;
struct AST; /* forward decl – avoids pulling in parser.h here */

/* Tactic-level value: a kernel expression, a pair, or a lazy AST thunk.
 * TVAL_AST is used for unevaluated tactic-call arguments: the AST is
 * resolved at use-time.  The AST pointer is NOT owned. */
typedef enum { TVAL_EXPRESSION, TVAL_PAIR, TVAL_AST } TacticValueKind;

struct TacticValue {
    TacticValueKind kind;
    union {
        Expression *expr;           /* TVAL_EXPRESSION */
        struct {
            TacticValue *fst;
            TacticValue *snd;
        } pair;                     /* TVAL_PAIR */
        struct AST *ast;            /* TVAL_AST – borrowed, never freed */
    };
};

static inline TacticValue *tactic_value_expr(Expression *e) {
    TacticValue *tv = malloc(sizeof(TacticValue));
    tv->kind = TVAL_EXPRESSION;
    tv->expr = e;
    return tv;
}

static inline TacticValue *tactic_value_pair(TacticValue *fst, TacticValue *snd) {
    TacticValue *tv = malloc(sizeof(TacticValue));
    tv->kind = TVAL_PAIR;
    tv->pair.fst = fst;
    tv->pair.snd = snd;
    return tv;
}

static inline TacticValue *tactic_value_ast(struct AST *a) {
    TacticValue *tv = malloc(sizeof(TacticValue));
    tv->kind = TVAL_AST;
    tv->ast = a;
    return tv;
}

/* Unwrap a TacticValue to Expression*, or NULL if it's a pair. */
static inline Expression *tactic_value_as_expr(TacticValue *tv) {
    if (!tv || tv->kind != TVAL_EXPRESSION) {
        return NULL;
    }
    return tv->expr;
}

struct TacticResult {
    bool success;                 // true if the tactic was successful, false otherwise
    DoublyLinkedList *new_goals;  // If success, the new goals, otherwise NULL
    char *error_message;          // If success, NULL, otherwise the error message
    TacticValue *term_value;      // Optional: tactic value returned by value-producing primitives
};

TacticResult *init_tactic_result(bool success, DoublyLinkedList *new_goals, char *error_message);
TacticResult *init_tactic_result_value(TacticValue *value);
void free_tactic_result(TacticResult *result);
void free_tactic_value(TacticValue *tv);
TacticValue *tactic_value_dup(TacticValue *tv);

/* Accessor functions */
bool tactic_result_get_success(TacticResult *result);
char *tactic_result_get_error_message(TacticResult *result);
DoublyLinkedList *tactic_result_get_goals(TacticResult *result);
TacticValue *tactic_result_get_value(TacticResult *result);

#endif  // TACTIC_API_H