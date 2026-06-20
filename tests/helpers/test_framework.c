#include "test_framework.h"

#include <stdio.h>
#include <string.h>

TestStats global_test_stats = {0, 0, 0};
static const char *current_suite_name = NULL;
static const char *current_test_name = NULL;
static bool current_test_passed = true;

void test_suite_start(const char *suite_name) {
    current_suite_name = suite_name;
    printf("\n=== %s ===\n\n", suite_name);
}

void test_suite_end(void) {
    current_suite_name = NULL;
    printf("\n");
}

void test_start(const char *test_name) {
    current_test_name = test_name;
    current_test_passed = true;
    global_test_stats.total_tests++;
}

static void test_fail(const char *message) {
    if (current_test_passed) {
        global_test_stats.failed_tests++;
        current_test_passed = false;
        printf("FAIL: %s\n", current_test_name);
    }
    printf("  %s\n", message);
}

static void test_pass(void) {
    if (current_test_passed) {
        global_test_stats.passed_tests++;
        printf("PASS: %s\n", current_test_name);
    }
}

void assert_true(bool condition, const char *message) {
    if (!condition) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected true, got false: %s", message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void assert_false(bool condition, const char *message) {
    if (condition) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected false, got true: %s", message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void assert_equal_int(int expected, int actual, const char *message) {
    if (expected != actual) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected %d, got %d: %s", expected, actual, message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void assert_equal_str(const char *expected, const char *actual, const char *message) {
    if (expected == NULL && actual == NULL) {
        if (current_test_passed) {
            test_pass();
        }
        return;
    }
    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected \"%s\", got \"%s\": %s",
                 expected ? expected : "(null)", actual ? actual : "(null)", message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void assert_not_null(const void *ptr, const char *message) {
    if (ptr == NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected non-null pointer: %s", message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void assert_null(const void *ptr, const char *message) {
    if (ptr != NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "Expected null pointer, got %p: %s", ptr, message);
        test_fail(buf);
    } else if (current_test_passed) {
        test_pass();
    }
}

void print_test_summary(void) {
    printf("\n========================================\n");
    printf("Test Summary:\n");
    printf("  Total:  %d\n", global_test_stats.total_tests);
    printf("  Passed: %d\n", global_test_stats.passed_tests);
    printf("  Failed: %d\n", global_test_stats.failed_tests);
    printf("========================================\n");
    printf("PASSED: %d\nFAILED: %d\n", global_test_stats.passed_tests,
           global_test_stats.failed_tests);
}

int get_test_failures(void) { return global_test_stats.failed_tests; }
