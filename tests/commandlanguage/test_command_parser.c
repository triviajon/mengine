#include "src/commandlanguage/command_parser.h"
#include "src/common/lexer.h"
#include "src/common/options.h"
#include "src/termlanguage/parser.h"
#include "tests/helpers/test_framework.h"

void test_parse_axiom_simple(void) {
    test_start("covers simple Axiom declaration");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Axiom x : Type.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DECLARATION, cmd->tag, "command should be DECLARATION");
    assert_equal_int(DECL_KW_AXIOM, cmd->as.decl.kw, "keyword should be AXIOM");
    assert_equal_str("x", cmd->as.decl.binder.name, "binder name should be 'x'");
    assert_not_null(cmd->as.decl.binder.type, "binder type should exist");
}

void test_parse_variable_declaration(void) {
    test_start("covers Variable declaration");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Variable y : Prop.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DECLARATION, cmd->tag, "command should be DECLARATION");
    assert_equal_int(DECL_KW_VARIABLE, cmd->as.decl.kw, "keyword should be VARIABLE");
    assert_equal_str("y", cmd->as.decl.binder.name, "binder name should be 'y'");
}

void test_parse_definition_no_params(void) {
    test_start("covers Definition with no parameters, boundary case");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Definition myType : Type := Prop.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("myType", cmd->as.defn.name, "name should be 'myType'");
    assert_equal_int(0, cmd->as.defn.param_count, "should have 0 parameters");
    assert_not_null(cmd->as.defn.type, "type should exist");
    assert_not_null(cmd->as.defn.body, "body should exist");
}

void test_parse_definition_single_param(void) {
    test_start("covers Definition with single parameter");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Definition id (x : Type) : Type := x.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("id", cmd->as.defn.name, "name should be 'id'");
    assert_equal_int(1, cmd->as.defn.param_count, "should have 1 parameter");
    assert_not_null(cmd->as.defn.params, "params should exist");
    assert_equal_str("x", cmd->as.defn.params[0]->name, "parameter name should be 'x'");
}

void test_parse_definition_multiple_params(void) {
    test_start("covers Definition with multiple parameters");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Definition const (A : Type) (x : A) : A := fun (y : A) => x.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("const", cmd->as.defn.name, "name should be 'const'");
    assert_equal_int(2, cmd->as.defn.param_count, "should have 2 parameters");
    assert_equal_str("A", cmd->as.defn.params[0]->name, "first param should be 'A'");
    assert_equal_str("x", cmd->as.defn.params[1]->name, "second param should be 'x'");
}

void test_parse_theorem(void) {
    test_start("covers Theorem statement");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Theorem t : forall (x : Type), x.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_STATEMENT, cmd->tag, "command should be STATEMENT");
    assert_equal_int(STMT_KW_THEOREM, cmd->as.stmt.kw, "keyword should be THEOREM");
    assert_equal_str("t", cmd->as.stmt.name, "name should be 't'");
    assert_not_null(cmd->as.stmt.type, "type should exist");
}

void test_parse_lemma(void) {
    test_start("covers Lemma statement");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Lemma add_comm : forall (n : nat), forall (m : nat), eq nat n m.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_STATEMENT, cmd->tag, "command should be STATEMENT");
    assert_equal_int(STMT_KW_LEMMA, cmd->as.stmt.kw, "keyword should be LEMMA");
    assert_equal_str("add_comm", cmd->as.stmt.name, "name should be 'add_comm'");
    assert_not_null(cmd->as.stmt.type, "type should exist");
}

void test_parse_axiom_extra_whitespace(void) {
    test_start("covers Axiom with extra whitespace");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Axiom   z   :   Type   .\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DECLARATION, cmd->tag, "command should be DECLARATION");
    assert_equal_str("z", cmd->as.decl.binder.name, "binder name should be 'z'");
}

void test_parse_variable_with_prop(void) {
    test_start("covers Variable with Prop type");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Variable P : Prop.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DECLARATION, cmd->tag, "command should be DECLARATION");
    assert_equal_str("P", cmd->as.decl.binder.name, "binder name should be 'P'");
}

void test_parse_definition_complex_type(void) {
    test_start("covers Definition with complex forall type");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx,
               "Definition myapply (A : Type) (B : Type) : forall (f : A), B := "
               "fun (f : A) => f.\n",
               &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("myapply", cmd->as.defn.name, "name should be 'myapply'");
    assert_equal_int(2, cmd->as.defn.param_count, "should have 2 parameters");
}

void test_parse_theorem_simple_type(void) {
    test_start("covers Theorem with simple Type");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Theorem simple : Type.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_STATEMENT, cmd->tag, "command should be STATEMENT");
    assert_equal_str("simple", cmd->as.stmt.name, "name should be 'simple'");
}

void test_parse_definition_lambda_body(void) {
    test_start("covers Definition with lambda body");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Definition f : Type := fun (x : Type) => x.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("f", cmd->as.defn.name, "name should be 'f'");
    assert_not_null(cmd->as.defn.body, "body should exist");
    assert_equal_int(AST_LAMBDA, cmd->as.defn.body->tag, "body should be lambda");
}

void test_parse_axiom_complex_type(void) {
    test_start("covers Axiom with complex forall type");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Axiom functional_extensionality : forall (A : Type), A.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DECLARATION, cmd->tag, "command should be DECLARATION");
    assert_equal_str("functional_extensionality", cmd->as.decl.binder.name,
                     "binder name should be 'functional_extensionality'");
}

void test_parse_lemma_complex(void) {
    test_start("covers Lemma with nested forall");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Lemma test : forall (A : Type), forall (B : Type), A.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_STATEMENT, cmd->tag, "command should be STATEMENT");
    assert_equal_str("test", cmd->as.stmt.name, "name should be 'test'");
    assert_not_null(cmd->as.stmt.type, "type should exist");
    assert_equal_int(AST_FORALL, cmd->as.stmt.type->tag, "type should be forall");
}

void test_parse_definition_identity(void) {
    test_start("covers Definition of identity function");

    MEngineOptions options = {.debug = false};
    Lexer lx;
    lexer_init(&lx, "Definition identity (A : Type) (x : A) : A := x.\n", &options);

    Parser parser;
    parser_init(&parser, &lx, &options);

    Command *cmd = command_parse_command(&parser);
    assert_not_null(cmd, "command should not be null");
    assert_equal_int(CMD_DEFINITION, cmd->tag, "command should be DEFINITION");
    assert_equal_str("identity", cmd->as.defn.name, "name should be 'identity'");
    assert_equal_int(2, cmd->as.defn.param_count, "should have 2 parameters");
    assert_equal_int(AST_VAR, cmd->as.defn.body->tag, "body should be variable");
}

int main() {
    test_suite_start("Command Parser Test Suite");

    // Declarations
    test_parse_axiom_simple();
    test_parse_variable_declaration();
    test_parse_axiom_extra_whitespace();
    test_parse_variable_with_prop();
    test_parse_axiom_complex_type();

    // Definitions - parameter count partition
    test_parse_definition_no_params();
    test_parse_definition_single_param();
    test_parse_definition_multiple_params();

    // Definitions - body types
    test_parse_definition_lambda_body();
    test_parse_definition_identity();
    test_parse_definition_complex_type();

    // Statements
    test_parse_theorem();
    test_parse_lemma();
    test_parse_theorem_simple_type();
    test_parse_lemma_complex();

    test_suite_end();
    print_test_summary();
    return get_test_failures();
}
