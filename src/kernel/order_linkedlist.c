#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/order.h"

/*
 * Linked-list implementation of the Order Data Structure.
 *  Insert - no-op: the context field set at construction encodes position
 *  Delete - no-op: freed by the GC with no extra bookkeeping required
 *  Order  - O(size(y)): walk from y toward the root checking if x is found.
 */

#ifdef ORDER_USE_LL

#if defined(ORDER_LL_STATS) || defined(ORDER_LL_TIMING)
#define ORDER_LL_INSTRUMENT 1
#endif

#ifdef ORDER_LL_INSTRUMENT
typedef struct {
    uint64_t order_queries;
    uint64_t order_steps;
    uint64_t max_order_steps;
    uint64_t total_order_query_ns;
} OrderLLStats;

static OrderLLStats g_order_ll_stats;
static bool g_order_ll_stats_registered = false;

static uint64_t order_ll_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void order_ll_print_stats(void) {
    double avg_steps = g_order_ll_stats.order_queries == 0
                           ? 0.0
                           : (double)g_order_ll_stats.order_steps /
                                 (double)g_order_ll_stats.order_queries;
    fprintf(stderr,
            "[order_ll] order_queries=%" PRIu64 " order_steps=%" PRIu64
            " avg_steps=%.3f max_steps=%" PRIu64 " time_ms=%.3f\n",
            g_order_ll_stats.order_queries,
            g_order_ll_stats.order_steps,
            avg_steps,
            g_order_ll_stats.max_order_steps,
            (double)g_order_ll_stats.total_order_query_ns / 1000000.0);
}

static void order_ll_register_stats(void) {
    if (!g_order_ll_stats_registered) {
        atexit(order_ll_print_stats);
        g_order_ll_stats_registered = true;
    }
}
#else
#define order_ll_register_stats() ((void)0)
#endif

void order_on_insert(Expression *parent, Expression *new_var) {
    (void)parent;
    (void)new_var;
}

void order_on_delete(Expression *var) { (void)var; }

bool order_precedes(Expression *x, Expression *y) {
#ifdef ORDER_LL_INSTRUMENT
    order_ll_register_stats();
    uint64_t start_ns = order_ll_now_ns();
    uint64_t steps = 0;
    g_order_ll_stats.order_queries++;
#endif
    Expression *curr = y;
    while (!context_is_empty(curr)) {
#ifdef ORDER_LL_INSTRUMENT
        steps++;
#endif
        if (curr == x) {
#ifdef ORDER_LL_INSTRUMENT
            g_order_ll_stats.order_steps += steps;
            if (steps > g_order_ll_stats.max_order_steps) {
                g_order_ll_stats.max_order_steps = steps;
            }
            g_order_ll_stats.total_order_query_ns += order_ll_now_ns() - start_ns;
#endif
            return true;
        }
        curr = get_expression_context(curr);
    }
#ifdef ORDER_LL_INSTRUMENT
    g_order_ll_stats.order_steps += steps;
    if (steps > g_order_ll_stats.max_order_steps) {
        g_order_ll_stats.max_order_steps = steps;
    }
    g_order_ll_stats.total_order_query_ns += order_ll_now_ns() - start_ns;
#endif
    return false;
}

#endif  // ORDER_USE_LL
