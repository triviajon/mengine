#include <argp.h>
#include <stdio.h>

#include "src/common/color.h"
#include "src/common/options.h"
#include "src/runtime/repl.h"
#include "src/runtime/runtime.h"

enum { OPT_PRINT_TOKENS = 256, OPT_PRINT_AST, OPT_PRINT_MODE };

static char doc[] = "MEngine - A theorem prover";
static char args_doc[] = "[FILENAME]";
static struct argp_option options[] = {
    {"debug", 'd', 0, 0, "Enable debug mode (default: false)", 0},
    {"load", 'l', "FILE", 0, "Load and execute FILE, then enter REPL", 0},
    {"print-tokens", OPT_PRINT_TOKENS, 0, 0,
     "Print tokens during parsing (default: true, requires --debug)", 0},
    {"print-ast", OPT_PRINT_AST, 0, 0, "Print AST during parsing (default: true, requires --debug)",
     0},
    {"print-mode", OPT_PRINT_MODE, 0, 0,
     "Print mode during execution (default: true, requires --debug)", 0},
    {0}};
struct arguments {
    char *filename;   // [FILENAME]
    char *load_file;  // -l/--load FILE
    bool debug, debug__print_tokens, debug__print_ast, debug__print_mode;
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
        case OPT_PRINT_TOKENS:
            arguments->debug__print_tokens = true;
            break;
        case OPT_PRINT_AST:
            arguments->debug__print_ast = true;
            break;
        case OPT_PRINT_MODE:
            arguments->debug__print_mode = true;
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

MEngineOptions build_options(struct arguments *args) {
    MEngineOptions options;
    options.debug = args->debug;
    options.debug__print_tokens = args->debug__print_tokens;
    options.debug__print_ast = args->debug__print_ast;
    options.debug__print_mode = args->debug__print_mode;
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

    // Default values
    arguments.debug = false;
    arguments.debug__print_tokens = true;
    arguments.debug__print_ast = true;
    arguments.debug__print_mode = true;
    arguments.filename = NULL;
    arguments.load_file = NULL;

    argp_parse(&argp, argc, argv, 0, 0, &arguments);
    MEngineOptions options = build_options(&arguments);

    if (arguments.load_file) {
        return load_and_repl_mode(options, arguments.load_file);
    }

    if (!arguments.filename) {
        return interactive_mode(options);
    }

    char *filename = arguments.filename;
    return file_mode(options, filename);
}
