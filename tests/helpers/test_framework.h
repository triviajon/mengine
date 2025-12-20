#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Test statistics
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} TestStats;

// Global test stats
extern TestStats global_test_stats;

// Test suite management
void test_suite_start(const char *suite_name);
void test_suite_end(void);

// Test assertions
void test_start(const char *test_name);
void assert_true(bool condition, const char *message);
void assert_false(bool condition, const char *message);
void assert_equal_int(int expected, int actual, const char *message);
void assert_equal_str(const char *expected, const char *actual,
                      const char *message);
void assert_not_null(const void *ptr, const char *message);
void assert_null(const void *ptr, const char *message);

// Test result reporting
void print_test_summary(void);
int get_test_failures(void);

#endif  // TEST_FRAMEWORK_H
