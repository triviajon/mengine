#include "src/kernel/expression_hash.h"

#include <stdint.h>
#include <string.h>

#include "src/common/hashcons_map.h"
#include "src/kernel/expression.h"

// Global intern table. Keys and values are both Expression*.
// Only APP, LAMBDA, FORALL expressions are interned.
// VAR and HOLE are mutable (bodies/fills can change) and are never interned.
// TYPE and PROP are permanent singletons handled separately.
static HashconsMap *expression_intern_table = NULL;

// ---------------------------------------------------------------------------
// Pointer hashing helpers
// ---------------------------------------------------------------------------

static uint32_t hash_ptr(const void *ptr) {
    uint64_t p = (uint64_t)(uintptr_t)ptr;
    p ^= p >> 33;
    p *= UINT64_C(0xff51afd7ed558ccd);
    p ^= p >> 33;
    p *= UINT64_C(0xc4ceb9fe1a85ec53);
    p ^= p >> 33;
    return (uint32_t)p;
}

static uint32_t hash_combine(uint32_t h1, uint32_t h2) {
    return h1 ^ (h2 * 2654435761u + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
}

// ---------------------------------------------------------------------------
// Hash and equality based solely on INPUT fields (not derived type/uplinks).
//
// Since all internable subexpressions (APP, LAMBDA, FORALL) are themselves
// interned, pointer equality on subexpressions implies structural equality.
// VAR and HOLE use identity (pointer) equality — each binding site is unique.
// ---------------------------------------------------------------------------

static uint32_t hash_inputs(const void *vexpr) {
    const Expression *expr = (const Expression *)vexpr;
    uint32_t h = (uint32_t)expr->tag * 2654435761u;
    switch (expr->tag) {
        case APP_EXPRESSION:
            h = hash_combine(h, hash_ptr(expr->context));
            h = hash_combine(h, hash_ptr(expr->as.app.func));
            h = hash_combine(h, hash_ptr(expr->as.app.arg));
            break;
        case LAMBDA_EXPRESSION:
            // Context is fully determined by bound_variable->context; no need to include it.
            h = hash_combine(h, hash_ptr(expr->as.lambda.bound_variable));
            h = hash_combine(h, hash_ptr(expr->as.lambda.body));
            break;
        case FORALL_EXPRESSION:
            h = hash_combine(h, hash_ptr(expr->as.forall.bound_variable));
            h = hash_combine(h, hash_ptr(expr->as.forall.body));
            break;
        default:
            // Non-internable types: use identity.
            h = hash_combine(h, hash_ptr(expr));
            break;
    }
    return h;
}

static bool eq_inputs(const void *va, const void *vb) {
    const Expression *a = (const Expression *)va;
    const Expression *b = (const Expression *)vb;
    if (a == b) return true;
    if (!a || !b || a->tag != b->tag) return false;
    switch (a->tag) {
        case APP_EXPRESSION:
            return a->context == b->context && a->as.app.func == b->as.app.func &&
                   a->as.app.arg == b->as.app.arg;
        case LAMBDA_EXPRESSION:
            return a->as.lambda.bound_variable == b->as.lambda.bound_variable &&
                   a->as.lambda.body == b->as.lambda.body;
        case FORALL_EXPRESSION:
            return a->as.forall.bound_variable == b->as.forall.bound_variable &&
                   a->as.forall.body == b->as.forall.body;
        default:
            return a == b;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void expression_intern_table_init(void) {
    if (!expression_intern_table) {
        expression_intern_table = hashcons_map_new(hash_inputs, eq_inputs);
    }
}

Expression *expression_intern_lookup(const Expression *probe) {
    expression_intern_table_init();
    return (Expression *)hashcons_map_get(expression_intern_table, probe);
}

void expression_intern_insert(Expression *expr) {
    expression_intern_table_init();
    hashcons_map_set(expression_intern_table, expr, expr);
}

void expression_intern_remove(const Expression *expr) {
    (void)expr; /* deferred GC: intern table freed at shutdown */
}

void expression_intern_table_free(void) {
    if (expression_intern_table) {
        hashcons_map_free(expression_intern_table);
        expression_intern_table = NULL;
    }
}
