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
 * Order-maintenance implementation using one-level indirection over an
 * Euler-tour list. Each variable owns two tokens: order_in (DFS entry) and
 * order_out (DFS exit). Tokens are grouped into small contiguous blocks with
 * local labels; blocks themselves are ordered by the tag-range relabeling
 * structure.
 *
 * order_precedes(x, y)  <->  x.in <= y.in  AND  y.out <= x.out
 */

#ifndef ORDER_USE_LL

#if defined(ORDER_TAGRANGE_STATS) || defined(ORDER_TAGRANGE_TIMING)
#define ORDER_TAGRANGE_INSTRUMENT 1
#endif

#ifndef ORDER_TAGRANGE_INSERT_STRATEGY
#define ORDER_TAGRANGE_INSERT_STRATEGY ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF
#endif

#define ORDER_TAGRANGE_STRATEGY_THIRDS 0
#define ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF 1

#if ORDER_TAGRANGE_INSERT_STRATEGY != ORDER_TAGRANGE_STRATEGY_THIRDS && \
    ORDER_TAGRANGE_INSERT_STRATEGY != ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF
#error "ORDER_TAGRANGE_INSERT_STRATEGY must be 0 (thirds) or 1 (plus-one-half)"
#endif

#if ORDER_TAGRANGE_INSERT_STRATEGY == ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF
#define ORDER_TAGRANGE_STRATEGY_NAME "plus-one-half"
#else
#define ORDER_TAGRANGE_STRATEGY_NAME "thirds"
#endif

#ifndef ORDER_TAGRANGE_THRESHOLD_T_NUM
#define ORDER_TAGRANGE_THRESHOLD_T_NUM 11
#endif

#ifndef ORDER_TAGRANGE_THRESHOLD_T_DEN
#define ORDER_TAGRANGE_THRESHOLD_T_DEN 10
#endif

#if ORDER_TAGRANGE_THRESHOLD_T_DEN <= 0
#error "ORDER_TAGRANGE_THRESHOLD_T_DEN must be positive"
#endif

#if ORDER_TAGRANGE_THRESHOLD_T_NUM <= ORDER_TAGRANGE_THRESHOLD_T_DEN
#error "ORDER_TAGRANGE_THRESHOLD_T must be greater than 1"
#endif

#if ORDER_TAGRANGE_THRESHOLD_T_NUM >= 2 * ORDER_TAGRANGE_THRESHOLD_T_DEN
#error "ORDER_TAGRANGE_THRESHOLD_T must be less than 2"
#endif

#ifdef ORDER_TAGRANGE_INSTRUMENT
typedef struct {
    uint64_t inserts;
    uint64_t fast_inserts;
    uint64_t repair_inserts;
    uint64_t block_splits;
    uint64_t block_reindex_tokens;
    uint64_t block_split_reindex_tokens;
    uint64_t block_split_membership_tokens;
    uint64_t block_gap_reindex_tokens;
    uint64_t block_init_reindex_tokens;
    uint64_t repair_tokens_retagged;
    uint64_t repair_interval_scans;
    uint64_t repair_interval_tokens;
    uint64_t order_queries;
    uint64_t total_insert_ns;
    uint64_t total_block_split_ns;
    uint64_t total_block_gap_reindex_ns;
    uint64_t total_fast_ns;
    uint64_t total_repair_interval_ns;
    uint64_t total_repair_relabel_ns;
    uint64_t total_order_query_ns;
} OrderTagrangeStats;

static OrderTagrangeStats g_order_tagrange_stats;
static bool g_order_tagrange_stats_registered = false;

static uint64_t order_tagrange_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double ns_to_ms(uint64_t ns) { return (double)ns / 1000000.0; }

static void order_tagrange_print_stats(void) {
    fflush(stdout);
    fprintf(stderr, "[order_tagrange] insert_strategy=%s\n", ORDER_TAGRANGE_STRATEGY_NAME);
    fprintf(stderr, "[order_tagrange] threshold_T=%d/%d\n", ORDER_TAGRANGE_THRESHOLD_T_NUM,
            ORDER_TAGRANGE_THRESHOLD_T_DEN);
    fprintf(stderr,
            "[order_tagrange] inserts=%" PRIu64 " top_fast=%" PRIu64 " top_repair=%" PRIu64
            " order_queries=%" PRIu64 "\n",
            g_order_tagrange_stats.inserts, g_order_tagrange_stats.fast_inserts,
            g_order_tagrange_stats.repair_inserts, g_order_tagrange_stats.order_queries);
    fprintf(stderr, "[order_tagrange] block_splits=%" PRIu64 " block_reindex_tokens=%" PRIu64 "\n",
            g_order_tagrange_stats.block_splits, g_order_tagrange_stats.block_reindex_tokens);
    fprintf(stderr,
            "[order_tagrange] block_reindex_split=%" PRIu64 " block_membership_split=%" PRIu64
            " block_reindex_gap=%" PRIu64 " block_reindex_init=%" PRIu64 "\n",
            g_order_tagrange_stats.block_split_reindex_tokens,
            g_order_tagrange_stats.block_split_membership_tokens,
            g_order_tagrange_stats.block_gap_reindex_tokens,
            g_order_tagrange_stats.block_init_reindex_tokens);
    fprintf(stderr,
            "[order_tagrange] top_repair_interval_scans=%" PRIu64
            " top_repair_interval_blocks=%" PRIu64 " top_blocks_retagged=%" PRIu64 "\n",
            g_order_tagrange_stats.repair_interval_scans, g_order_tagrange_stats.repair_interval_tokens,
            g_order_tagrange_stats.repair_tokens_retagged);
    fprintf(stderr,
            "[order_tagrange] time_ms insert=%.3f fast=%.3f repair_interval=%.3f"
            " repair_relabel=%.3f split=%.3f gap_reindex=%.3f order_query=%.3f\n",
            ns_to_ms(g_order_tagrange_stats.total_insert_ns),
            ns_to_ms(g_order_tagrange_stats.total_fast_ns),
            ns_to_ms(g_order_tagrange_stats.total_repair_interval_ns),
            ns_to_ms(g_order_tagrange_stats.total_repair_relabel_ns),
            ns_to_ms(g_order_tagrange_stats.total_block_split_ns),
            ns_to_ms(g_order_tagrange_stats.total_block_gap_reindex_ns),
            ns_to_ms(g_order_tagrange_stats.total_order_query_ns));
}

static void order_tagrange_register_stats(void) {
    if (!g_order_tagrange_stats_registered) {
        atexit(order_tagrange_print_stats);
        g_order_tagrange_stats_registered = true;
    }
}
#else
#define order_tagrange_register_stats() ((void)0)
#endif

static uint64_t g_order_tagrange_thresholds[64];
static bool g_order_tagrange_thresholds_ready = false;

static void init_thresholds(void) {
    if (g_order_tagrange_thresholds_ready) {
        return;
    }

    long double value = 1.0L;
    long double ratio = (2.0L * (long double)ORDER_TAGRANGE_THRESHOLD_T_DEN) /
                        (long double)ORDER_TAGRANGE_THRESHOLD_T_NUM;
    g_order_tagrange_thresholds[0] = 1;
    for (int k = 1; k < 64; k++) {
        value *= ratio;
        uint64_t threshold = (uint64_t)value;
        if (threshold == 0) {
            threshold = 1;
        }
        g_order_tagrange_thresholds[k] = threshold;
    }
    g_order_tagrange_thresholds_ready = true;
}

static inline uint64_t threshold(int k) {
    init_thresholds();
    return g_order_tagrange_thresholds[k];
}

struct OrderBlock {
    uint64_t tag;
    struct OrderBlock *prev;
    struct OrderBlock *next;
    OrderToken *first;
    OrderToken *last;
    int count;
};

#ifndef ORDER_TAGRANGE_BLOCK_CAPACITY
#define ORDER_TAGRANGE_BLOCK_CAPACITY 256
#endif

#if ORDER_TAGRANGE_BLOCK_CAPACITY < 8
#error "ORDER_TAGRANGE_BLOCK_CAPACITY must be at least 8"
#endif

static OrderToken root_out;
static OrderBlock *g_root_block;

static inline OrderToken *var_order_in(Expression *var) {
    assert(var->tag == VAR_EXPRESSION);
    return &var->as.var.order_in;
}

static inline OrderToken *var_order_out(Expression *var) {
    assert(var->tag == VAR_EXPRESSION);
    return &var->as.var.order_out;
}

static inline OrderBlock *block_next(OrderBlock *block) { return block->next; }
static inline OrderBlock *block_prev(OrderBlock *block) { return block->prev; }
static inline uint64_t block_tag(OrderBlock *block) { return block->tag; }

static OrderBlock *order_block_new(OrderToken *first, OrderToken *last, int count) {
    OrderBlock *block = calloc(1, sizeof(OrderBlock));
    if (!block) {
        return NULL;
    }
    block->first = first;
    block->last = last;
    block->count = count;
    return block;
}

static void block_reindex(OrderBlock *block) {
    uint64_t gap = UINT64_MAX / (uint64_t)(block->count + 1);
    assert(gap > 0);
    int index = 0;
    for (OrderToken *token = block->first; token != NULL; token = token->next) {
        token->block = block;
        token->local_tag = (uint64_t)(index + 1) * gap;
        index++;
        if (token == block->last) {
            break;
        }
    }
    assert(index == block->count);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.block_reindex_tokens += (uint64_t)block->count;
#endif
}

static void block_reindex_for_split(OrderBlock *block) {
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t before = g_order_tagrange_stats.block_reindex_tokens;
#endif
    block_reindex(block);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.block_split_reindex_tokens +=
        g_order_tagrange_stats.block_reindex_tokens - before;
#endif
}

static void block_reindex_for_gap(OrderBlock *block) {
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t start_ns = order_tagrange_now_ns();
    uint64_t before = g_order_tagrange_stats.block_reindex_tokens;
#endif
    block_reindex(block);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.block_gap_reindex_tokens +=
        g_order_tagrange_stats.block_reindex_tokens - before;
    g_order_tagrange_stats.total_block_gap_reindex_ns += order_tagrange_now_ns() - start_ns;
#endif
}

static void block_reindex_for_init(OrderBlock *block) {
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t before = g_order_tagrange_stats.block_reindex_tokens;
#endif
    block_reindex(block);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.block_init_reindex_tokens +=
        g_order_tagrange_stats.block_reindex_tokens - before;
#endif
}

static void block_reassign_membership_for_split(OrderBlock *block) {
    int index = 0;
    for (OrderToken *token = block->first; token != NULL; token = token->next) {
        token->block = block;
        index++;
        if (token == block->last) {
            break;
        }
    }
    assert(index == block->count);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.block_split_membership_tokens += (uint64_t)block->count;
#endif
}

static void ensure_root_block(OrderToken *root_token) {
    if (root_token->block != NULL) {
        return;
    }

    OrderBlock *block = order_block_new(root_token, root_token, 1);
    assert(block != NULL);
    block->tag = UINT64_MAX;
    g_root_block = block;
    block_reindex_for_init(block);
}

static void find_block_repair_interval(OrderBlock *pivot, OrderBlock **out_left, int *out_count,
                                       uint64_t *out_lower, uint64_t *out_upper) {
    uint64_t tag = block_tag(pivot);
    OrderBlock *left = pivot;
    OrderBlock *right = pivot;
    int count = 1;

    for (int k = 1; k <= 63; k++) {
        uint64_t size = 1ULL << k;
        uint64_t mask = size - 1;
        uint64_t lower = tag & ~mask;
        uint64_t upper = (k == 63 || lower > UINT64_MAX - size) ? UINT64_MAX : lower + size;

        // Extend cursors outward — each node is visited at most once total
        while (block_prev(left) != NULL && block_tag(block_prev(left)) >= lower) {
            left = block_prev(left);
            count++;
#ifdef ORDER_TAGRANGE_INSTRUMENT
            g_order_tagrange_stats.repair_interval_scans++;
#endif
        }
        while (block_next(right) != NULL && block_tag(block_next(right)) < upper) {
            right = block_next(right);
            count++;
#ifdef ORDER_TAGRANGE_INSTRUMENT
            g_order_tagrange_stats.repair_interval_scans++;
#endif
        }

        if ((uint64_t)(count + 1) <= threshold(k)) {
            *out_left = left;
            *out_count = count;
            *out_lower = lower;
            *out_upper = upper;
            return;
        }
    }

    // Worst case: entire list
    while (block_prev(left) != NULL) {
        left = block_prev(left);
        count++;
#ifdef ORDER_TAGRANGE_INSTRUMENT
        g_order_tagrange_stats.repair_interval_scans++;
#endif
    }
    while (block_next(right) != NULL) {
        right = block_next(right);
        count++;
#ifdef ORDER_TAGRANGE_INSTRUMENT
        g_order_tagrange_stats.repair_interval_scans++;
#endif
    }
    *out_left = left;
    *out_count = count;
    *out_lower = 0;
    *out_upper = UINT64_MAX;
}

static void block_link_before(OrderBlock *succ_block, OrderBlock *new_block) {
    OrderBlock *prev_block = block_prev(succ_block);
    uint64_t succ_tag = block_tag(succ_block);
    uint64_t prev_tag = (prev_block != NULL) ? block_tag(prev_block) : 0;

    if (succ_tag - prev_tag > 1) {
#ifdef ORDER_TAGRANGE_INSTRUMENT
        uint64_t fast_start_ns = order_tagrange_now_ns();
        g_order_tagrange_stats.fast_inserts++;
#endif
#if ORDER_TAGRANGE_INSERT_STRATEGY == ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF
        new_block->tag = prev_tag + 1;
#else
        new_block->tag = prev_tag + (succ_tag - prev_tag) / 2;
#endif
        new_block->prev = prev_block;
        new_block->next = succ_block;
        if (prev_block != NULL) {
            prev_block->next = new_block;
        }
        succ_block->prev = new_block;
#ifdef ORDER_TAGRANGE_INSTRUMENT
        g_order_tagrange_stats.total_fast_ns += order_tagrange_now_ns() - fast_start_ns;
#endif
        return;
    }

#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.repair_inserts++;
    uint64_t interval_start_ns = order_tagrange_now_ns();
#endif
    OrderBlock *pivot = (prev_block != NULL) ? prev_block : succ_block;
    OrderBlock *left;
    int count;
    uint64_t lower;
    uint64_t upper;
    find_block_repair_interval(pivot, &left, &count, &lower, &upper);
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t interval_end_ns = order_tagrange_now_ns();
    g_order_tagrange_stats.total_repair_interval_ns += interval_end_ns - interval_start_ns;
    g_order_tagrange_stats.repair_interval_tokens += (uint64_t)count;
    uint64_t relabel_start_ns = interval_end_ns;
#endif

    int total = count + 1;  // existing + one new block
    uint64_t gap = (upper - lower) / (uint64_t)(total + 1);
    assert(gap >= 1);

    OrderBlock *boundary_prev = left->prev;
    OrderBlock *chain_head = left;
    OrderBlock *curr = left;
    OrderBlock *list_prev = NULL;
    int i = 1;
    bool inserted = false;

    while (curr != NULL && (upper == UINT64_MAX || block_tag(curr) < upper)) {
        if (!inserted && curr == succ_block) {
            new_block->tag = lower + (uint64_t)i++ * gap;
            new_block->prev = list_prev;
            if (list_prev != NULL) {
                list_prev->next = new_block;
            } else {
                chain_head = new_block;
            }
            list_prev = new_block;
            inserted = true;
        }

        OrderBlock *saved_next = curr->next;
        curr->tag = lower + (uint64_t)i++ * gap;
        curr->prev = list_prev;
        if (list_prev != NULL) {
            list_prev->next = curr;
        }
        list_prev = curr;
        curr = saved_next;
#ifdef ORDER_TAGRANGE_INSTRUMENT
        g_order_tagrange_stats.repair_tokens_retagged++;
#endif
    }

    // succ_block was outside (or at the end of) the interval
    if (!inserted) {
        new_block->tag = lower + (uint64_t)i++ * gap;
        new_block->prev = list_prev;
        if (list_prev != NULL) {
            list_prev->next = new_block;
        }
        list_prev = new_block;
    }

    OrderBlock *boundary_next = curr;  // first block past the interval
    if (list_prev != NULL) {
        list_prev->next = boundary_next;
    }
    if (boundary_next != NULL) {
        boundary_next->prev = list_prev;
    }
    if (boundary_prev != NULL) {
        boundary_prev->next = chain_head;
    }
    chain_head->prev = boundary_prev;
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t repair_end_ns = order_tagrange_now_ns();
    g_order_tagrange_stats.total_repair_relabel_ns += repair_end_ns - relabel_start_ns;
#endif
}

static void split_block_if_needed(OrderBlock *block, bool preserve_local_tags) {
    while (block->count > ORDER_TAGRANGE_BLOCK_CAPACITY) {
#ifdef ORDER_TAGRANGE_INSTRUMENT
        uint64_t split_start_ns = order_tagrange_now_ns();
        g_order_tagrange_stats.block_splits++;
#endif
        int left_count = block->count / 2;
        OrderToken *left_last = block->first;
        for (int i = 1; i < left_count; i++) {
            left_last = left_last->next;
        }
        OrderToken *right_first = left_last->next;

        OrderBlock *left_block = order_block_new(block->first, left_last, left_count);
        assert(left_block != NULL);
        block->first = right_first;
        block->count -= left_count;

        block_link_before(block, left_block);
        if (preserve_local_tags) {
            block_reassign_membership_for_split(left_block);
        } else {
            block_reindex_for_split(left_block);
            block_reindex_for_split(block);
        }
#ifdef ORDER_TAGRANGE_INSTRUMENT
        g_order_tagrange_stats.total_block_split_ns += order_tagrange_now_ns() - split_start_ns;
#endif
    }
}

static void insert_token_pair_before(OrderToken *succ_tok, OrderToken *in_tok,
                                     OrderToken *out_tok) {
    ensure_root_block(succ_tok);
    OrderBlock *block = succ_tok->block;
    OrderToken *prev_tok = succ_tok->prev;
    uint64_t prev_index = (prev_tok != NULL && prev_tok->block == block) ? prev_tok->local_tag : 0;
    uint64_t succ_index = succ_tok->local_tag;

    in_tok->prev = prev_tok;
    in_tok->next = out_tok;
    out_tok->prev = in_tok;
    out_tok->next = succ_tok;
    if (prev_tok != NULL) {
        prev_tok->next = in_tok;
    }
    succ_tok->prev = out_tok;

    if (block->first == succ_tok) {
        block->first = in_tok;
    }
    block->count += 2;
    bool assigned_local_tags = false;
    if (succ_index - prev_index > 2) {
#if ORDER_TAGRANGE_INSERT_STRATEGY == ORDER_TAGRANGE_STRATEGY_PLUS_ONE_HALF
        in_tok->local_tag = prev_index + 1;
        out_tok->local_tag = in_tok->local_tag + (succ_index - in_tok->local_tag) / 2;
#else
        uint64_t gap = (succ_index - prev_index) / 3;
        in_tok->local_tag = prev_index + gap;
        out_tok->local_tag = prev_index + 2 * gap;
#endif
        in_tok->block = block;
        out_tok->block = block;
        assigned_local_tags = true;
    }

    if (block->count > ORDER_TAGRANGE_BLOCK_CAPACITY) {
        split_block_if_needed(block, assigned_local_tags);
    } else if (!assigned_local_tags) {
        in_tok->block = block;
        out_tok->block = block;
        block_reindex_for_gap(block);
    }
}

void order_on_insert(Expression *parent, Expression *new_var) {
    order_tagrange_register_stats();
#ifdef ORDER_TAGRANGE_INSTRUMENT
    uint64_t insert_start_ns = order_tagrange_now_ns();
    g_order_tagrange_stats.inserts++;
#endif
    OrderToken *succ_tok =
        (parent != NULL && !context_is_empty(parent)) ? var_order_out(parent) : &root_out;
    insert_token_pair_before(succ_tok, var_order_in(new_var), var_order_out(new_var));
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.total_insert_ns += order_tagrange_now_ns() - insert_start_ns;
#endif
}

static void remove_block_if_empty(OrderBlock *block) {
    if (block->count != 0) {
        return;
    }
    if (block->prev != NULL) {
        block->prev->next = block->next;
    }
    if (block->next != NULL) {
        block->next->prev = block->prev;
    }
    if (g_root_block == block) {
        g_root_block = block->next != NULL ? block->next : block->prev;
    }
    free(block);
}

static void remove_token(OrderToken *token) {
    OrderBlock *block = token->block;
    if (block == NULL) {
        return;
    }
    if (token->prev != NULL) {
        token->prev->next = token->next;
    }
    if (token->next != NULL) {
        token->next->prev = token->prev;
    }
    if (block->first == token) {
        block->first = token->next;
    }
    if (block->last == token) {
        block->last = token->prev;
    }
    block->count--;
    token->prev = NULL;
    token->next = NULL;
    token->block = NULL;
    if (block->count == 0) {
        remove_block_if_empty(block);
    }
}

void order_on_delete(Expression *var) {
    remove_token(var_order_in(var));
    remove_token(var_order_out(var));
}

static bool token_precedes(OrderToken *x, OrderToken *y) {
    if (x->block == y->block) {
        return x->local_tag <= y->local_tag;
    }
    return block_tag(x->block) <= block_tag(y->block);
}

bool order_precedes(Expression *x, Expression *y) {
    if (context_is_empty(x)) {
        return true;
    }
    if (context_is_empty(y)) {
        return false;
    }
#ifdef ORDER_TAGRANGE_INSTRUMENT
    order_tagrange_register_stats();
    uint64_t start_ns = order_tagrange_now_ns();
    g_order_tagrange_stats.order_queries++;
#endif
    bool result = token_precedes(var_order_in(x), var_order_in(y)) &&
                  token_precedes(var_order_out(y), var_order_out(x));
#ifdef ORDER_TAGRANGE_INSTRUMENT
    g_order_tagrange_stats.total_order_query_ns += order_tagrange_now_ns() - start_ns;
#endif
    return result;
}

#endif  // ORDER_USE_LL
