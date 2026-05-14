#ifndef TIMING_H
#define TIMING_H

#include <stdbool.h>

typedef enum {
    TIMER_PARSE,
    TIMER_COMMAND,
    TIMER_TACTIC,
    TIMER_ENGINE,
    TIMER_KERNEL,
    TIMER_COUNT,
} TimerSection;

/* Enable or disable all timing. Must be called before any timer_push/pop. */
void timer_set_enabled(bool enabled);

/* Push a new active section onto the stack.
 * Stops accumulating time for the current section and starts for s. */
void timer_push(TimerSection s);

/* Pop the current section, resuming the previous one. */
void timer_pop(void);

/* Print a summary to stderr. */
void timer_print_summary(void);

#endif /* TIMING_H */
