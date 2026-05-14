#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>

#include "src/common/color.h"
#include "src/common/options.h"
#include "src/common/timing.h"
#include "src/runtime/repl.h"
#include "src/runtime/runtime.h"

enum { OPT_PRINT_TOKENS = 256, OPT_PRINT_AST, OPT_PRINT_MODE, OPT_TIME };

static char doc[] =
    "MEngine - A theorem prover\n\nCommands:\n  config    Print compile-time configuration and "
    "exit";
static char args_doc[] = "[FILENAME|config]";
static struct argp_option options[] = {
    {"load", 'l', "FILE", 0, "Load and execute FILE, then enter REPL", 0},
    {"quiet", 'q', 0, 0, "Suppress text output", 0},
    {"debug", 'd', 0, 0, "Enable debug mode (default: false)", 0},
    {"print-tokens", OPT_PRINT_TOKENS, 0, 0,
     "Print tokens during parsing (default: true, requires --debug)", 0},
    {"print-ast", OPT_PRINT_AST, 0, 0, "Print AST during parsing (default: true, requires --debug)",
     0},
    {"print-mode", OPT_PRINT_MODE, 0, 0,
     "Print mode during execution (default: true, requires --debug)", 0},
    {"time", OPT_TIME, 0, 0, "Print subsystem timing summary on exit", 0},
    {0}};
struct arguments {
    char *filename;   // [FILENAME]
    char *load_file;  // -l/--load FILE
    bool quiet, debug, debug__print_tokens, debug__print_ast, debug__print_mode, time;
};
static struct argp_child children[] = {{0}};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    struct arguments *arguments = state->input;

    switch (key) {
        case 'd':
            arguments->debug = true;
            break;
        case 'l':
            arguments->load_file = arg;
            break;
        case 'q':
            arguments->quiet = true;
            break;
        case OPT_PRINT_TOKENS:
            arguments->debug__print_tokens = true;
            break;
        case OPT_PRINT_AST:
            arguments->debug__print_ast = true;
            break;
        case OPT_PRINT_MODE:
            arguments->debug__print_mode = true;
            break;
        case OPT_TIME:
            arguments->time = true;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num > 0) {
                // Too many arguments
                argp_usage(state);
            }
            arguments->filename = arg;
            break;
        case ARGP_KEY_END:
            break;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc, children, 0, 0};

static void raise_stack_limit(void) {
    struct rlimit rl;
    if (getrlimit(RLIMIT_STACK, &rl) != 0) {
        return;
    }

    const rlim_t target = (rlim_t)64 * 1024 * 1024;  // 64 MiB
    if (rl.rlim_cur >= target) {
        return;
    }

    rlim_t new_cur = target;
    if (rl.rlim_max != RLIM_INFINITY && new_cur > rl.rlim_max) {
        new_cur = rl.rlim_max;
    }
    if (new_cur <= rl.rlim_cur) {
        return;
    }

    struct rlimit updated = rl;
    updated.rlim_cur = new_cur;
    (void)setrlimit(RLIMIT_STACK, &updated);
}

MEngineOptions build_options(struct arguments *args) {
    MEngineOptions options;
    options.debug = args->debug;
    options.debug__print_tokens = args->debug__print_tokens;
    options.debug__print_ast = args->debug__print_ast;
    options.debug__print_mode = args->debug__print_mode;
    options.quiet = args->quiet;
    options.time = args->time;
    return options;
}

int interactive_mode(MEngineOptions options) {
    MEngineRuntime *rt = mengine_runtime_new(&options);
    if (!rt) {
        fprintf(stderr, ERROR "Failed to initialize MEngine runtime.\n" CRESET);
        return 1;
    }

    mengine_repl(rt);
    mengine_runtime_free(rt);

    return 0;
}

int file_mode(MEngineOptions options, char *filename) {
    MEngineRuntime *rt = mengine_runtime_new(&options);
    if (!rt) {
        fprintf(stderr, ERROR "Failed to initialize MEngine runtime.\n" CRESET);
        return 1;
    }

    int r = mengine_runtime_exec_file(rt, filename);
    mengine_runtime_free(rt);

    return r;
}

static void config_mode(void) {
    printf("mengine compile-time configuration\n");
    printf("----------------------------------\n");

#ifndef ORDER_USE_LL
    printf("ORDER_IMPL           : tag-range relabeling\n");
#ifndef ORDER_DEMAIN_INSERT_STRATEGY
#define ORDER_DEMAIN_INSERT_STRATEGY 0
#endif
#ifndef ORDER_DEMAIN_THRESHOLD_T_NUM
#define ORDER_DEMAIN_THRESHOLD_T_NUM 11
#endif
#ifndef ORDER_DEMAIN_THRESHOLD_T_DEN
#define ORDER_DEMAIN_THRESHOLD_T_DEN 10
#endif
    printf("ORDER_DEMAIN_STRATEGY: %s\n",
           ORDER_DEMAIN_INSERT_STRATEGY == 1 ? "plus-one-half" : "thirds");
    printf("ORDER_DEMAIN_T        : %d/%d\n", ORDER_DEMAIN_THRESHOLD_T_NUM,
           ORDER_DEMAIN_THRESHOLD_T_DEN);
#ifdef ORDER_DEMAIN_INSTRUMENT
    printf("ORDER_DEMAIN_STATS   : enabled\n");
#elif defined(ORDER_DEMAIN_STATS) || defined(ORDER_DEMAIN_TIMING)
    printf("ORDER_DEMAIN_STATS   : enabled\n");
#else
    printf("ORDER_DEMAIN_STATS   : disabled\n");
#endif
#else
    printf("ORDER_IMPL           : linked-list\n");
#endif
}

int load_and_repl_mode(MEngineOptions options, char *filename) {
    MEngineRuntime *rt = mengine_runtime_new(&options);
    if (!rt) {
        fprintf(stderr, ERROR "Failed to initialize MEngine runtime.\n" CRESET);
        return 1;
    }

    int r = mengine_runtime_exec_file(rt, filename);
    if (r != 0) {
        mengine_runtime_free(rt);
        return r;
    }

    mengine_repl(rt);
    mengine_runtime_free(rt);

    return 0;
}

int main(int argc, char **argv) {
    struct arguments arguments;

    raise_stack_limit();

    // Default values
    arguments.quiet = false;
    arguments.debug = false;
    arguments.debug__print_tokens = true;
    arguments.debug__print_ast = true;
    arguments.debug__print_mode = true;
    arguments.time = false;
    arguments.filename = NULL;
    arguments.load_file = NULL;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    MEngineOptions options = build_options(&arguments);
    timer_set_enabled(options.time);

    if (arguments.load_file) {
        int rc = load_and_repl_mode(options, arguments.load_file);
        timer_print_summary();
        return rc;
    }

    if (!arguments.filename) {
        int rc = interactive_mode(options);
        timer_print_summary();
        return rc;
    }

    if (strcmp(arguments.filename, "config") == 0) {
        config_mode();
        return 0;
    }

    char *filename = arguments.filename;
    int rc = file_mode(options, filename);
    timer_print_summary();
    return rc;
}
