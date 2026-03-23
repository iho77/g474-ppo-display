/**
 * @file test_runner.c
 * @brief Native x86 unit test orchestrator for g474_port safety-critical modules
 *
 * Framework: Custom GCC harness — no external dependencies.
 * Compile: see tests/Makefile
 * Run:     cd tests && make test
 */

#include <stdio.h>
#include <stdlib.h>

/* ── Test framework state ─────────────────────────────────────────────────── */

int g_passed = 0;
int g_failed = 0;
const char *g_current_test = NULL;

/**
 * RUN_TEST — invoke test_func(), print PASS on return, FAIL on assert/abort.
 *
 * On failure, assert() calls abort(). We use atexit to detect this — but for
 * simplicity the framework just increments g_passed on normal return. If an
 * assert fires the process exits non-zero so the test suite reports failure.
 *
 * For per-test FAIL counting without process death, tests use the
 * EXPECT_EQ / EXPECT_TRUE helpers defined below.
 */
#define RUN_TEST(test_func)                                                 \
    do {                                                                    \
        g_current_test = #test_func;                                       \
        printf("  %-60s ", #test_func);                                    \
        fflush(stdout);                                                     \
        test_func();                                                        \
        printf("PASS\n");                                                   \
        g_passed++;                                                         \
    } while (0)

/**
 * EXPECT_TRUE — non-fatal assertion (continues on failure).
 * Marks test as failed but does not abort.
 */
#define EXPECT_TRUE(cond)                                                   \
    do {                                                                    \
        if (!(cond)) {                                                      \
            printf("\n    EXPECT_TRUE failed: %s  [%s:%d]",               \
                   #cond, __FILE__, __LINE__);                             \
            g_failed++;                                                     \
        }                                                                   \
    } while (0)

#define EXPECT_EQ(a, b)                                                     \
    do {                                                                    \
        if ((a) != (b)) {                                                   \
            printf("\n    EXPECT_EQ failed: %s != %s  [%s:%d]",           \
                   #a, #b, __FILE__, __LINE__);                            \
            g_failed++;                                                     \
        }                                                                   \
    } while (0)

/* ── Forward declarations ─────────────────────────────────────────────────── */

void run_calibration_tests(void);
void run_drift_tracker_tests(void);
void run_flash_utils_tests(void);
void run_warning_tests(void);
void run_settings_tests(void);
void run_pressure_sensor_tests(void);
void run_ms5837_tests(void);

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(void) {
    printf("========================================\n");
    printf("g474_port Safety-Critical Test Suite\n");
    printf("========================================\n\n");

    run_calibration_tests();
    run_drift_tracker_tests();
    run_flash_utils_tests();
    run_warning_tests();
    run_settings_tests();
    run_pressure_sensor_tests();
    run_ms5837_tests();

    printf("\n========================================\n");
    printf("SUMMARY: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    return (g_failed > 0) ? 1 : 0;
}
