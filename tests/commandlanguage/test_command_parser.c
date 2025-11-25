#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/commandlanguage/parser.h"
#include "src/common/lexer.h"
#include "src/metalanguage/parser.h"

void print_binder(const Binder *b) {
    if (!b) {
        printf("(null-binder)");
        return;
    }
    printf("%s : ", b->name);
    printf("<term@%p>", (void *)b->type);
}

void print_ast_brief(const AST *ast) {
    if (!ast) {
        printf("NULL");
        return;
    }
    switch (ast->tag) {
        case AST_VAR:
            printf("VAR(%s)", ast->value.var.name);
            break;
        case AST_TYPE:
            printf("TYPE");
            break;
        case AST_PROP:
            printf("PROP");
            break;
        case AST_LAMBDA:
            printf("LAMBDA@%p", (void *)ast);
            break;
        case AST_FORALL:
            printf("FORALL@%p", (void *)ast);
            break;
        case AST_APP:
            printf("APP@%p", (void *)ast);
            break;
        case AST_MATCH:
            printf("MATCH@%p", (void *)ast);
            break;
        default:
            printf("AST@%p", (void *)ast);
    }
}

void print_command(const Command *cmd) {
    if (!cmd) {
        printf("(null-command)");
        return;
    }

    switch (cmd->tag) {
        case CMD_DECLARATION:
            printf("DECLARATION(");
            printf(cmd->as.decl.kw == DECL_KW_AXIOM ? "Axiom, " : "Variable, ");
            print_binder(&cmd->as.decl.binder);
            printf(")");
            break;

        case CMD_DEFINITION:
            printf("DEFINITION(");
            printf("%s, params=[", cmd->as.defn.name);
            for (size_t i = 0; i < cmd->as.defn.param_count; i++) {
                print_binder(cmd->as.defn.params[i]);
                if (i + 1 < cmd->as.defn.param_count) printf("; ");
            }
            printf("], type=");
            print_ast_brief(cmd->as.defn.type);
            printf(", body=");
            print_ast_brief(cmd->as.defn.body);
            printf(")");
            break;

        case CMD_STATEMENT:
            printf("STATEMENT(");
            printf(cmd->as.stmt.kw == STMT_KW_THEOREM ? "Theorem, "
                                                      : "Lemma, ");
            printf("%s, params=[", cmd->as.stmt.name);
            for (size_t i = 0; i < cmd->as.stmt.param_count; i++) {
                print_binder(cmd->as.stmt.params[i]);
                if (i + 1 < cmd->as.stmt.param_count) printf("; ");
            }
            printf("], type=");
            print_ast_brief(cmd->as.stmt.type);
            printf(")");
            break;

        default:
            printf("UNKNOWN_COMMAND");
            break;
    }
}

static void test_command_parsing(const char *input, const char *description) {
    printf("Test: %s\n", description);
    printf("Input: \"%s\"\n", input);
    printf("Command: ");

    Lexer lx;
    lexer_init(&lx, input);

    Parser parser;
    parser_init(&parser, &lx);

    Command *cmd = parse_command(&parser);
    print_command(cmd);
    printf("\n\n");
}

/* ------------------------------------------------------------------ */
int main() {
    printf("=== Command Parser Test Suite ===\n\n");

    test_command_parsing("Axiom x : Type.", "Simple axiom declaration");
    test_command_parsing("Variable y : Prop.",
                         "Variable declaration with Prop");
    test_command_parsing("Definition id (x : Type) : Type := x.",
                         "Simple identity definition");
    test_command_parsing(
        "Definition const (A : Type) (x : A) : A := fun y : A => x.",
        "Definition with multiple parameters and lambda body");
    test_command_parsing("Theorem t : forall x : Type , x.",
                         "Simple theorem with a forall");
    test_command_parsing("Lemma add_comm (n : nat) (m : nat) : eq nat n m.",
                         "Lemma with parameters");
    test_command_parsing("Axiom   z   :   Type   .",
                         "Declaration with extra whitespace");

    printf("All command parser tests completed!\n");
    return 0;
}
