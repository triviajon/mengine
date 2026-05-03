#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
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

static inline OrderToken *ot_next(OrderToken *ot) { return ot->next; }
static inline OrderToken *ot_prev(OrderToken *ot) { return ot->prev; }
static inline uint64_t    ot_tag (OrderToken *ot) { return ot->tag;  }

static inline uint64_t threshold(int k) {
    uint64_t size = 1ULL << k;
    return size - size / (2 * (uint64_t)k);
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
        }
        while (ot_next(right) != NULL && ot_tag(ot_next(right)) < upper) {
            right = ot_next(right);
            count++;
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
    while (ot_prev(left)  != NULL) { left  = ot_prev(left);  count++; }
    while (ot_next(right) != NULL) { right = ot_next(right); count++; }
    *out_left  = left;
    *out_count = count;
    *out_lower = 0;
    *out_upper = UINT64_MAX;
}

void order_on_insert(Expression *parent, Expression *new_var) {
    OrderToken *succ_tok = (parent != NULL) ? &parent->order_out : &root_out;
    OrderToken *prev_tok = ot_prev(succ_tok);
    uint64_t succ_tag = ot_tag(succ_tok);
    uint64_t prev_tag = (prev_tok != NULL) ? ot_tag(prev_tok) : 0;

    // Room for both in and out tokens
    if (succ_tag - prev_tag > 2) {
        uint64_t gap = (succ_tag - prev_tag) / 3;
        new_var->order_in.tag  = prev_tag + gap;
        new_var->order_out.tag = prev_tag + 2 * gap;
        new_var->order_in.prev  = prev_tok;
        new_var->order_in.next  = &new_var->order_out;
        new_var->order_out.prev = &new_var->order_in;
        new_var->order_out.next = succ_tok;
        if (prev_tok != NULL) prev_tok->next = &new_var->order_in;
        succ_tok->prev = &new_var->order_out;
        return;
    }

    // Need to relabel tokens
    OrderToken *pivot = (prev_tok != NULL) ? prev_tok : succ_tok;
    OrderToken *left;
    int count;
    uint64_t lower, upper;
    find_repair_interval(pivot, &left, &count, &lower, &upper);

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
    return x->order_in.tag  <= y->order_in.tag &&
           y->order_out.tag <= x->order_out.tag;
}

#endif  // ORDER_USE_LL


