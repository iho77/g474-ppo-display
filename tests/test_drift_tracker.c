/**
 * @file test_drift_tracker.c
 * @brief Unit tests for drift_tracker.c
 *
 * Tested scenarios:
 *  1.  Init → all fields zeroed
 *  2.  Single sample → no slope calculated (count < 3)
 *  3.  Two samples → no slope calculated
 *  4.  Three samples (flat) → slope = 0
 *  5.  Three samples (rising) → positive slope
 *  6.  Three samples (falling) → negative slope
 *  7.  Full window (18 samples) → sample_count saturates at DRIFT_WINDOW_SIZE
 *  8.  Circular buffer wraps correctly after 18 samples
 *  9.  drift_get_rate_per_min returns mbar/min (×6 conversion)
 * 10.  drift_tracker_calculate_slope with corrupted sample_count → safe reset
 * 11.  Linear regression correctness: constant +6mbar/min per-interval gives
 *      drift_rate == +6 mbar/min (1 interval/step × 6 = 6)
 * 12.  Multi-sensor independence (separate tracker structs)
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drift_tracker.h"
#include "sensor.h"

extern int g_passed;
extern int g_failed;
extern const char *g_current_test;

#define RUN_TEST(test_func)                                             \
    do {                                                                \
        g_current_test = #test_func;                                   \
        printf("  %-60s ", #test_func);                                \
        fflush(stdout);                                                 \
        test_func();                                                    \
        printf("PASS\n");                                               \
        g_passed++;                                                     \
    } while (0)

/* ── Helpers ──────────────────────────────────────────────────────────────── */

static drift_tracker_t make_tracker(void) {
    drift_tracker_t t;
    memset(&t, 0, sizeof(t));
    return t;
}

/* Feed n identical samples to tracker */
static void feed_flat(volatile drift_tracker_t *t, int32_t val, int n) {
    for (int i = 0; i < n; i++) {
        drift_tracker_sample(t, val);
    }
}

/* Feed samples linearly increasing by delta each step */
static void feed_linear(volatile drift_tracker_t *t, int32_t start, int32_t delta, int n) {
    for (int i = 0; i < n; i++) {
        drift_tracker_sample(t, start + (int32_t)i * delta);
    }
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_drift_init_zeroes_all_fields(void) {
    drift_tracker_t t;
    memset(&t, 0xFF, sizeof(t));   /* Poison memory */
    drift_tracker_init((volatile drift_tracker_t*)&t);

    assert(t.buffer_index == 0);
    assert(t.sample_count == 0);
    assert(t.drift_rate_mbar_per_min == 0);
    for (int i = 0; i < DRIFT_WINDOW_SIZE; i++) {
        assert(t.ppo_history[i] == 0);
    }
}

static void test_drift_one_sample_no_slope(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 700);

    assert(t.sample_count == 1);
    assert(t.drift_rate_mbar_per_min == 0);
}

static void test_drift_two_samples_no_slope(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    feed_flat((volatile drift_tracker_t*)&t, 700, 2);

    assert(t.sample_count == 2);
    assert(t.drift_rate_mbar_per_min == 0);
}

static void test_drift_three_flat_samples_zero_slope(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    feed_flat((volatile drift_tracker_t*)&t, 700, 3);

    assert(t.sample_count == 3);
    assert(t.drift_rate_mbar_per_min == 0);
}

static void test_drift_three_rising_samples_positive_slope(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    /* 700, 706, 712 → slope = 6 per interval → *6 = 36 mbar/min (3 samples)
     * With n=3: sum_x=3, denom=6
     * sum_y=2118, sum_xy = 0*700 + 1*706 + 2*712 = 0+706+1424 = 2130
     * numerator = 3*2130 - 3*2118 = 6390 - 6354 = 36
     * slope_per_interval = 36/6 = 6
     * drift_rate = 6 * 6 = 36 mbar/min */
    drift_tracker_sample((volatile drift_tracker_t*)&t, 700);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 706);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 712);

    assert(t.drift_rate_mbar_per_min > 0);
    assert(t.drift_rate_mbar_per_min == 36);
}

static void test_drift_three_falling_samples_negative_slope(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 712);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 706);
    drift_tracker_sample((volatile drift_tracker_t*)&t, 700);

    assert(t.drift_rate_mbar_per_min < 0);
    assert(t.drift_rate_mbar_per_min == -36);
}

static void test_drift_sample_count_saturates_at_window(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    /* Feed more than DRIFT_WINDOW_SIZE samples */
    feed_flat((volatile drift_tracker_t*)&t, 700, DRIFT_WINDOW_SIZE + 5);

    assert(t.sample_count == DRIFT_WINDOW_SIZE);
}

static void test_drift_circular_buffer_wraps(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);

    /* Fill to capacity with value 500, then add DRIFT_WINDOW_SIZE more at 700 */
    feed_flat((volatile drift_tracker_t*)&t, 500, DRIFT_WINDOW_SIZE);
    feed_flat((volatile drift_tracker_t*)&t, 700, DRIFT_WINDOW_SIZE);

    /* After full overwrite, slope should be 0 (all samples == 700) */
    assert(t.drift_rate_mbar_per_min == 0);
}

static void test_drift_get_rate_per_min_returns_stored_value(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    feed_flat((volatile drift_tracker_t*)&t, 700, 3);

    int32_t rate = drift_get_rate_per_min((const volatile drift_tracker_t*)&t);
    assert(rate == t.drift_rate_mbar_per_min);
}

static void test_drift_calculate_with_corrupted_count_safe(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    /* Corrupt the sample count beyond DRIFT_WINDOW_SIZE */
    t.sample_count = DRIFT_WINDOW_SIZE + 1;
    drift_tracker_calculate_slope((volatile drift_tracker_t*)&t);

    /* Must reset to safe state, not crash */
    assert(t.drift_rate_mbar_per_min == 0);
    assert(t.sample_count == 0);
}

static void test_drift_regression_constant_rate(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    /*
     * Feed samples at exactly +1 mbar per 10-second interval = +6 mbar/min
     * With integer arithmetic (slope = 1 per interval × 6 = 6 mbar/min)
     */
    feed_linear((volatile drift_tracker_t*)&t, 700, 1, DRIFT_WINDOW_SIZE);

    /* Drift rate should be exactly 6 mbar/min (1 interval × 6 intervals/min) */
    assert(t.drift_rate_mbar_per_min == 6);
}

static void test_drift_calculate_slope_below_three_returns_zero(void) {
    drift_tracker_t t = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t);
    t.sample_count = 2;  /* Below minimum */
    drift_tracker_calculate_slope((volatile drift_tracker_t*)&t);

    assert(t.drift_rate_mbar_per_min == 0);
}

static void test_drift_two_independent_trackers(void) {
    drift_tracker_t t1 = make_tracker(), t2 = make_tracker();
    drift_tracker_init((volatile drift_tracker_t*)&t1);
    drift_tracker_init((volatile drift_tracker_t*)&t2);

    /* t1: rising, t2: flat */
    feed_linear((volatile drift_tracker_t*)&t1, 700, 6, 5);
    feed_flat((volatile drift_tracker_t*)&t2, 700, 5);

    assert(t1.drift_rate_mbar_per_min > 0);
    assert(t2.drift_rate_mbar_per_min == 0);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_drift_tracker_tests(void) {
    printf("\n[DRIFT TRACKER TESTS - drift_tracker.c]\n");
    RUN_TEST(test_drift_init_zeroes_all_fields);
    RUN_TEST(test_drift_one_sample_no_slope);
    RUN_TEST(test_drift_two_samples_no_slope);
    RUN_TEST(test_drift_three_flat_samples_zero_slope);
    RUN_TEST(test_drift_three_rising_samples_positive_slope);
    RUN_TEST(test_drift_three_falling_samples_negative_slope);
    RUN_TEST(test_drift_sample_count_saturates_at_window);
    RUN_TEST(test_drift_circular_buffer_wraps);
    RUN_TEST(test_drift_get_rate_per_min_returns_stored_value);
    RUN_TEST(test_drift_calculate_with_corrupted_count_safe);
    RUN_TEST(test_drift_regression_constant_rate);
    RUN_TEST(test_drift_calculate_slope_below_three_returns_zero);
    RUN_TEST(test_drift_two_independent_trackers);
}
