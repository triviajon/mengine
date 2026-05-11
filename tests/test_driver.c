#include "tests/helpers/test_framework.h"

void run_evar_refs_tests(void);
void run_order_tests(void);
void run_conversion_tests(void);
void run_command_parser_tests(void);
void run_parser_tests(void);
void run_lexer_tests(void);
void run_ast_to_expression_tests(void);
void run_integration_tests(void);

int main(void) {
    run_evar_refs_tests();
    run_order_tests();
    run_conversion_tests();
    run_command_parser_tests();
    run_parser_tests();
    run_lexer_tests();
    run_ast_to_expression_tests();
    run_integration_tests();

    print_test_summary();
    return get_test_failures() > 0 ? 1 : 0;
}
