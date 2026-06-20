#include "src/common/timing.h"

#include <stdio.h>
#include <time.h>

#define TIMING_STACK_MAX 64

static bool g_enabled = false;

static long long g_accum_ns[TIMER_COUNT];
static int g_stack[TIMING_STACK_MAX];
static int g_stack_top = 0;     /* index of the next free slot */
static struct timespec g_start; /* when the current top section started */

static const char *section_name(TimerSection s) {
    switch (s) {
        case TIMER_PARSE:
            return "parse";
        case TIMER_COMMAND:
            return "command";
        case TIMER_TACTIC:
            return "tactic";
        case TIMER_ENGINE:
            return "engine";
        case TIMER_KERNEL:
            return "kernel";
        default:
            return "unknown";
    }
}

void timer_set_enabled(bool enabled) { g_enabled = enabled; }

static long long elapsed_ns(struct timespec *since) {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    return (long long)(now.tv_sec - since->tv_sec) * 1000000000LL + (now.tv_nsec - since->tv_nsec);
}

void timer_push(TimerSection s) {
    if (!g_enabled) { return;
}
    if (g_stack_top >= TIMING_STACK_MAX) { return;
}

    /* Flush time elapsed so far to the current top section. */
    if (g_stack_top > 0) {
        g_accum_ns[g_stack[g_stack_top - 1]] += elapsed_ns(&g_start);
    }

    g_stack[g_stack_top++] = (int)s;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &g_start);
}

void timer_pop(void) {
    if (!g_enabled) { return;
}
    if (g_stack_top == 0) { return;
}

    g_accum_ns[g_stack[g_stack_top - 1]] += elapsed_ns(&g_start);
    g_stack_top--;

    /* Resume the parent section's clock. */
    if (g_stack_top > 0) {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &g_start);
    }
}

void timer_print_summary(void) {
    if (!g_enabled) { return;
}

    long long total_ns = 0;
    for (int i = 0; i < TIMER_COUNT; i++) {
        total_ns += g_accum_ns[i];
    }

    fprintf(stderr, "\ntiming summary\n");
    fprintf(stderr, "──────────────────────────\n");
    for (int i = 0; i < TIMER_COUNT; i++) {
        long long ns = g_accum_ns[i];
        double ms = (double)ns / 1e6;
        double pct = total_ns > 0 ? (double)ns / (double)total_ns * 100.0 : 0.0;
        fprintf(stderr, "  %-8s  %8.3f ms  (%5.1f%%)\n", section_name(i), ms, pct);
    }
    fprintf(stderr, "──────────────────────────\n");
    fprintf(stderr, "  %-8s  %8.3f ms\n", "total", (double)total_ns / 1e6);
}
