#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "src/kernel/context.h"
#include "src/kernel/expression.h"
#include "src/kernel/order.h"

/*
 * Tag-range relabeling implementation of the Order Data Structure.
 * Uses an Euler-tour doubly-linked list where each Expression contributes
 * two OrderTokens: order_in (DFS entry) and order_out (DFS exit).
 *
 * order_precedes(x, y)  <->  x.in.tag <= y.in.tag  AND  y.out.tag <= x.out.tag
 *
 * Insert - O(log n) amortized
 * Delete - O(1)
 * Order  - O(1)
 */

#ifndef ORDER_USE_LL

#if defined(ORDER_DEMAIN_STATS) || defined(ORDER_DEMAIN_TIMING)
#define ORDER_DEMAIN_INSTRUMENT 1
#endif

#ifndef ORDER_DEMAIN_INSERT_STRATEGY
#define ORDER_DEMAIN_INSERT_STRATEGY 0
#endif

#define ORDER_DEMAIN_STRATEGY_THIRDS 0
#define ORDER_DEMAIN_STRATEGY_PLUS_ONE_HALF 1

#if ORDER_DEMAIN_INSERT_STRATEGY != ORDER_DEMAIN_STRATEGY_THIRDS && \
    ORDER_DEMAIN_INSERT_STRATEGY != ORDER_DEMAIN_STRATEGY_PLUS_ONE_HALF
#error "ORDER_DEMAIN_INSERT_STRATEGY must be 0 (thirds) or 1 (plus-one-half)"
#endif

#if ORDER_DEMAIN_INSERT_STRATEGY == ORDER_DEMAIN_STRATEGY_PLUS_ONE_HALF
#define ORDER_DEMAIN_STRATEGY_NAME "plus-one-half"
#else
#define ORDER_DEMAIN_STRATEGY_NAME "thirds"
#endif

#ifndef ORDER_DEMAIN_THRESHOLD_T_NUM
#define ORDER_DEMAIN_THRESHOLD_T_NUM 11
#endif

#ifndef ORDER_DEMAIN_THRESHOLD_T_DEN
#define ORDER_DEMAIN_THRESHOLD_T_DEN 10
#endif

#if ORDER_DEMAIN_THRESHOLD_T_DEN <= 0
#error "ORDER_DEMAIN_THRESHOLD_T_DEN must be positive"
#endif

#if ORDER_DEMAIN_THRESHOLD_T_NUM <= ORDER_DEMAIN_THRESHOLD_T_DEN
#error "ORDER_DEMAIN_THRESHOLD_T must be greater than 1"
#endif

#if ORDER_DEMAIN_THRESHOLD_T_NUM >= 2 * ORDER_DEMAIN_THRESHOLD_T_DEN
#error "ORDER_DEMAIN_THRESHOLD_T must be less than 2"
#endif

#ifdef ORDER_DEMAIN_INSTRUMENT
typedef struct {
    uint64_t inserts;
    uint64_t fast_inserts;
    uint64_t repair_inserts;
    uint64_t repair_tokens_retagged;
    uint64_t repair_interval_scans;
    uint64_t repair_interval_tokens;
    uint64_t order_queries;
    uint64_t total_insert_ns;
    uint64_t total_fast_ns;
    uint64_t total_repair_interval_ns;
    uint64_t total_repair_relabel_ns;
    uint64_t total_order_query_ns;
} OrderDemainStats;

static OrderDemainStats g_order_demain_stats;
static bool g_order_demain_stats_registered = false;

static uint64_t order_demain_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double ns_to_ms(uint64_t ns) { return (double)ns / 1000000.0; }

static void order_demain_print_stats(void) {
    fflush(stdout);
    fprintf(stderr, "[order_demain] insert_strategy=%s\n", ORDER_DEMAIN_STRATEGY_NAME);
    fprintf(stderr, "[order_demain] threshold_T=%d/%d\n",
            ORDER_DEMAIN_THRESHOLD_T_NUM, ORDER_DEMAIN_THRESHOLD_T_DEN);
    fprintf(stderr,
            "[order_demain] inserts=%" PRIu64 " fast=%" PRIu64 " repair=%" PRIu64
            " order_queries=%" PRIu64 "\n",
            g_order_demain_stats.inserts,
            g_order_demain_stats.fast_inserts,
            g_order_demain_stats.repair_inserts,
            g_order_demain_stats.order_queries);
    fprintf(stderr,
            "[order_demain] repair_interval_scans=%" PRIu64
            " repair_interval_tokens=%" PRIu64
            " repair_tokens_retagged=%" PRIu64 "\n",
            g_order_demain_stats.repair_interval_scans,
            g_order_demain_stats.repair_interval_tokens,
            g_order_demain_stats.repair_tokens_retagged);
    fprintf(stderr,
            "[order_demain] time_ms insert=%.3f fast=%.3f repair_interval=%.3f"
            " repair_relabel=%.3f order_query=%.3f\n",
            ns_to_ms(g_order_demain_stats.total_insert_ns),
            ns_to_ms(g_order_demain_stats.total_fast_ns),
            ns_to_ms(g_order_demain_stats.total_repair_interval_ns),
            ns_to_ms(g_order_demain_stats.total_repair_relabel_ns),
            ns_to_ms(g_order_demain_stats.total_order_query_ns));
}

static void order_demain_register_stats(void) {
    if (!g_order_demain_stats_registered) {
        atexit(order_demain_print_stats);
        g_order_demain_stats_registered = true;
    }
}
#else
#define order_demain_register_stats() ((void)0)
#endif

static inline OrderToken *ot_next(OrderToken *ot) { return ot->next; }
static inline OrderToken *ot_prev(OrderToken *ot) { return ot->prev; }
static inline uint64_t    ot_tag (OrderToken *ot) { return ot->tag;  }

static uint64_t g_order_demain_thresholds[64];
static bool g_order_demain_thresholds_ready = false;

static void init_thresholds(void) {
    if (g_order_demain_thresholds_ready) {
        return;
    }

    long double value = 1.0L;
    long double ratio = (2.0L * (long double)ORDER_DEMAIN_THRESHOLD_T_DEN) /
                        (long double)ORDER_DEMAIN_THRESHOLD_T_NUM;
    g_order_demain_thresholds[0] = 1;
    for (int k = 1; k < 64; k++) {
        value *= ratio;
        uint64_t threshold = (uint64_t)value;
        if (threshold == 0) {
            threshold = 1;
        }
        g_order_demain_thresholds[k] = threshold;
    }
    g_order_demain_thresholds_ready = true;
}

static inline uint64_t threshold(int k) {
    init_thresholds();
    return g_order_demain_thresholds[k];
}

static OrderToken root_out;

static void find_repair_interval(OrderToken *pivot,
                                 OrderToken **out_left, int *out_count,
                                 uint64_t *out_lower, uint64_t *out_upper) {
    uint64_t tag = ot_tag(pivot);
    OrderToken *left  = pivot;
    OrderToken *right = pivot;
    int count = 1;

    for (int k = 1; k <= 63; k++) {
        uint64_t size  = 1ULL << k;
        uint64_t mask  = size - 1;
        uint64_t lower = tag & ~mask;
        uint64_t upper = lower + size;

        // Extend cursors outward — each node is visited at most once total
        while (ot_prev(left) != NULL && ot_tag(ot_prev(left)) >= lower) {
            left = ot_prev(left);
            count++;
#ifdef ORDER_DEMAIN_INSTRUMENT
            g_order_demain_stats.repair_interval_scans++;
#endif
        }
        while (ot_next(right) != NULL && ot_tag(ot_next(right)) < upper) {
            right = ot_next(right);
            count++;
#ifdef ORDER_DEMAIN_INSTRUMENT
            g_order_demain_stats.repair_interval_scans++;
#endif
        }

        if ((uint64_t)(count + 2) <= threshold(k)) {
            *out_left  = left;
            *out_count = count;
            *out_lower = lower;
            *out_upper = upper;
            return;
        }
    }

    // Worst case: entire list
    while (ot_prev(left)  != NULL) {
        left  = ot_prev(left);
        count++;
#ifdef ORDER_DEMAIN_INSTRUMENT
        g_order_demain_stats.repair_interval_scans++;
#endif
    }
    while (ot_next(right) != NULL) {
        right = ot_next(right);
        count++;
#ifdef ORDER_DEMAIN_INSTRUMENT
        g_order_demain_stats.repair_interval_scans++;
#endif
    }
    *out_left  = left;
    *out_count = count;
    *out_lower = 0;
    *out_upper = UINT64_MAX;
}

void order_on_insert(Expression *parent, Expression *new_var) {
    order_demain_register_stats();
#ifdef ORDER_DEMAIN_INSTRUMENT
    uint64_t insert_start_ns = order_demain_now_ns();
    g_order_demain_stats.inserts++;
#endif
    OrderToken *succ_tok = (parent != NULL) ? &parent->order_out : &root_out;
    OrderToken *prev_tok = ot_prev(succ_tok);
    uint64_t succ_tag = ot_tag(succ_tok);
    uint64_t prev_tag = (prev_tok != NULL) ? ot_tag(prev_tok) : 0;

    // Room for both in and out tokens
    if (succ_tag - prev_tag > 2) {
#ifdef ORDER_DEMAIN_INSTRUMENT
        uint64_t fast_start_ns = order_demain_now_ns();
        g_order_demain_stats.fast_inserts++;
#endif
#if ORDER_DEMAIN_INSERT_STRATEGY == ORDER_DEMAIN_STRATEGY_PLUS_ONE_HALF
        new_var->order_in.tag = prev_tag + 1;
        new_var->order_out.tag = new_var->order_in.tag + (succ_tag - new_var->order_in.tag) / 2;
#else
        uint64_t gap = (succ_tag - prev_tag) / 3;
        new_var->order_in.tag  = prev_tag + gap;
        new_var->order_out.tag = prev_tag + 2 * gap;
#endif
        new_var->order_in.prev  = prev_tok;
        new_var->order_in.next  = &new_var->order_out;
        new_var->order_out.prev = &new_var->order_in;
        new_var->order_out.next = succ_tok;
        if (prev_tok != NULL) prev_tok->next = &new_var->order_in;
        succ_tok->prev = &new_var->order_out;
#ifdef ORDER_DEMAIN_INSTRUMENT
        uint64_t fast_end_ns = order_demain_now_ns();
        g_order_demain_stats.total_fast_ns += fast_end_ns - fast_start_ns;
        g_order_demain_stats.total_insert_ns += fast_end_ns - insert_start_ns;
#endif
        return;
    }

    // Need to relabel tokens
#ifdef ORDER_DEMAIN_INSTRUMENT
    g_order_demain_stats.repair_inserts++;
#endif
    OrderToken *pivot = (prev_tok != NULL) ? prev_tok : succ_tok;
    OrderToken *left;
    int count;
    uint64_t lower, upper;
#ifdef ORDER_DEMAIN_INSTRUMENT
    uint64_t interval_start_ns = order_demain_now_ns();
#endif
    find_repair_interval(pivot, &left, &count, &lower, &upper);
#ifdef ORDER_DEMAIN_INSTRUMENT
    uint64_t interval_end_ns = order_demain_now_ns();
    g_order_demain_stats.total_repair_interval_ns += interval_end_ns - interval_start_ns;
    g_order_demain_stats.repair_interval_tokens += (uint64_t)count;
    uint64_t relabel_start_ns = interval_end_ns;
#endif

    int total = count + 2; // existing + two new tokens
    uint64_t gap = (upper - lower) / (uint64_t)(total + 1);
    assert(gap >= 1);

    // Single forward walk: relabel existing tokens and splice in the two new
    // ones when we reach succ_tok, all without any auxiliary allocation.
    OrderToken *boundary_prev = left->prev;
    OrderToken *curr = left;
    OrderToken *list_prev = NULL; // previous token in the rewritten chain
    int i = 1;
    bool inserted = false;

    while (curr != NULL && (upper == UINT64_MAX || ot_tag(curr) < upper)) {
        if (!inserted && curr == succ_tok) {
            new_var->order_in.tag  = lower + (uint64_t)i++ * gap;
            new_var->order_out.tag = lower + (uint64_t)i++ * gap;
            new_var->order_in.prev  = list_prev;
            new_var->order_in.next  = &new_var->order_out;
            new_var->order_out.prev = &new_var->order_in;
            if (list_prev != NULL) list_prev->next = &new_var->order_in;
            else if (boundary_prev != NULL) boundary_prev->next = &new_var->order_in;
            list_prev = &new_var->order_out;
            inserted = true;
        }

        OrderToken *saved_next = curr->next;
        curr->tag  = lower + (uint64_t)i++ * gap;
        curr->prev = list_prev;
        if (list_prev != NULL) list_prev->next = curr;
        list_prev = curr;
        curr = saved_next;
#ifdef ORDER_DEMAIN_INSTRUMENT
        g_order_demain_stats.repair_tokens_retagged++;
#endif
    }

    // succ_tok was outside (or at the end of) the interval
    if (!inserted) {
        new_var->order_in.tag  = lower + (uint64_t)i++ * gap;
        new_var->order_out.tag = lower + (uint64_t)i++ * gap;
        new_var->order_in.prev  = list_prev;
        new_var->order_in.next  = &new_var->order_out;
        new_var->order_out.prev = &new_var->order_in;
        if (list_prev != NULL) list_prev->next = &new_var->order_in;
        list_prev = &new_var->order_out;
    }

    // Reconnect the rewritten chain to the rest of the list
    OrderToken *boundary_next = curr; // first token past the interval
    if (list_prev != NULL) list_prev->next = boundary_next;
    if (boundary_next != NULL) boundary_next->prev = list_prev;
    if (boundary_prev != NULL) boundary_prev->next = left;
    left->prev = boundary_prev;
#ifdef ORDER_DEMAIN_INSTRUMENT
    uint64_t repair_end_ns = order_demain_now_ns();
    g_order_demain_stats.total_repair_relabel_ns += repair_end_ns - relabel_start_ns;
    g_order_demain_stats.total_insert_ns += repair_end_ns - insert_start_ns;
#endif
}

void order_on_delete(Expression *var) {
    // Unlink both tokens
    OrderToken *in  = &var->order_in;
    OrderToken *out = &var->order_out;
    if (in->prev  != NULL) in->prev->next  = in->next;
    if (in->next  != NULL) in->next->prev  = in->prev;
    if (out->prev != NULL) out->prev->next = out->next;
    if (out->next != NULL) out->next->prev = out->prev;
}

bool order_precedes(Expression *x, Expression *y) {
#ifdef ORDER_DEMAIN_INSTRUMENT
    order_demain_register_stats();
    uint64_t start_ns = order_demain_now_ns();
    g_order_demain_stats.order_queries++;
#endif
    bool result = x->order_in.tag  <= y->order_in.tag &&
                  y->order_out.tag <= x->order_out.tag;
#ifdef ORDER_DEMAIN_INSTRUMENT
    g_order_demain_stats.total_order_query_ns += order_demain_now_ns() - start_ns;
#endif
    return result;
}

#endif  // ORDER_USE_LL
