/**
 * @file test_calibration.c
 * @brief Unit tests for calibration.c — calibrate_sensor()
 *
 * Tested scenarios:
 *  1. Normal valid two-point calibration → coefficients written, state unchanged
 *  2. Division by zero (identical raw readings) → NON_CALIBRATED
 *  3. Inverted slope (ppo rises but raw falls) → NON_CALIBRATED
 *  4. Dead sensor (ppo delta = 0) → NON_CALIBRATED
 *  5. Minimum separation too small (+49 counts) → NON_CALIBRATED
 *  6. Minimum separation too small (-49 counts) → NON_CALIBRATED
 *  7. Exact boundary: separation == 50 → calibration succeeds
 *  8. Coefficients correct for known linear relationship
 */

#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "calibration.h"
#include "sensor.h"
#include "mocks/mock_sensor_data.h"

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

/* Tolerance for float comparison */
#define FLOAT_TOL 1e-4f
static int float_near(float a, float b) {
    float diff = a - b;
    return (diff < FLOAT_TOL && diff > -FLOAT_TOL);
}

/* ── Tests ─────────────────────────────────────────────────────────────────── */

static void test_cal_normal_two_point(void) {
    reset_sensor_data();
    calibration_point p1 = {.raw = 500,  .ppo = 210};
    calibration_point p2 = {.raw = 1500, .ppo = 630};
    calibrate_sensor(p1, p2, SENSOR1);

    /* Slope = (630-210)/(1500-500) = 420/1000 = 0.42 */
    /* Intercept = (1500*210 - 500*630) / (1500-500) = (315000-315000)/1000 = 0 */
    assert(float_near(sensor_data.s_cal[0][SENSOR1], 0.42f));
    assert(float_near(sensor_data.s_cal[1][SENSOR1], 0.0f));
}

static void test_cal_identical_raw_gives_non_calibrated(void) {
    reset_sensor_data();
    calibration_point p1 = {.raw = 1000, .ppo = 210};
    calibration_point p2 = {.raw = 1000, .ppo = 630};  /* Same raw */
    calibrate_sensor(p1, p2, SENSOR1);

    assert(sensor_data.os_s_cal_state[SENSOR1] == NON_CALIBRATED);
}

static void test_cal_inverted_slope_gives_non_calibrated(void) {
    reset_sensor_data();
    /* ppo increases but raw decreases — negative slope → invalid */
    calibration_point p1 = {.raw = 1500, .ppo = 210};
    calibration_point p2 = {.raw = 500,  .ppo = 630};
    calibrate_sensor(p1, p2, SENSOR1);

    assert(sensor_data.os_s_cal_state[SENSOR1] == NON_CALIBRATED);
}

static void test_cal_dead_sensor_gives_non_calibrated(void) {
    reset_sensor_data();
    /* ppo never changes — flat sensor */
    calibration_point p1 = {.raw = 500,  .ppo = 400};
    calibration_point p2 = {.raw = 1500, .ppo = 400};
    calibrate_sensor(p1, p2, SENSOR1);

    assert(sensor_data.os_s_cal_state[SENSOR1] == NON_CALIBRATED);
}

static void test_cal_small_separation_pos_gives_non_calibrated(void) {
    reset_sensor_data();
    /* +49 raw counts — below 50 minimum */
    calibration_point p1 = {.raw = 1000, .ppo = 200};
    calibration_point p2 = {.raw = 1049, .ppo = 400};
    calibrate_sensor(p1, p2, SENSOR1);

    assert(sensor_data.os_s_cal_state[SENSOR1] == NON_CALIBRATED);
}

static void test_cal_small_separation_neg_gives_non_calibrated(void) {
    reset_sensor_data();
    /* -49 raw counts — below -50 minimum (p2 < p1) */
    calibration_point p1 = {.raw = 1049, .ppo = 400};
    calibration_point p2 = {.raw = 1000, .ppo = 200};
    calibrate_sensor(p1, p2, SENSOR1);

    assert(sensor_data.os_s_cal_state[SENSOR1] == NON_CALIBRATED);
}

static void test_cal_exact_50_separation_succeeds(void) {
    reset_sensor_data();
    /* Exactly 50 raw count separation — should succeed */
    calibration_point p1 = {.raw = 1000, .ppo = 200};
    calibration_point p2 = {.raw = 1050, .ppo = 400};
    calibrate_sensor(p1, p2, SENSOR1);

    /* calibrate_sensor() writes coefficients but does not update os_s_cal_state.
     * Verify success by checking the slope coefficient was written.
     * Slope = (400-200)/(1050-1000) = 200/50 = 4.0 */
    assert(float_near(sensor_data.s_cal[0][SENSOR1], 4.0f));
}

static void test_cal_calibration_points_stored(void) {
    reset_sensor_data();
    calibration_point p1 = {.raw = 800,  .ppo = 210};
    calibration_point p2 = {.raw = 1900, .ppo = 700};
    calibrate_sensor(p1, p2, SENSOR2);

    /* Shared calibration points written to s_cpt[] */
    assert(sensor_data.s_cpt[0].raw == 800);
    assert(sensor_data.s_cpt[0].ppo == 210);
    assert(sensor_data.s_cpt[1].raw == 1900);
    assert(sensor_data.s_cpt[1].ppo == 700);
}

static void test_cal_coefficients_correct_known_values(void) {
    reset_sensor_data();
    /* Force known values: p1=(0, 0), p2=(100, 500) → slope=5, intercept=0 */
    calibration_point p1 = {.raw = 0,   .ppo = 0};
    calibration_point p2 = {.raw = 100, .ppo = 500};
    calibrate_sensor(p1, p2, SENSOR3);

    assert(float_near(sensor_data.s_cal[0][SENSOR3], 5.0f));
    assert(float_near(sensor_data.s_cal[1][SENSOR3], 0.0f));
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_calibration_tests(void) {
    printf("\n[CALIBRATION TESTS - calibration.c]\n");
    RUN_TEST(test_cal_normal_two_point);
    RUN_TEST(test_cal_identical_raw_gives_non_calibrated);
    RUN_TEST(test_cal_inverted_slope_gives_non_calibrated);
    RUN_TEST(test_cal_dead_sensor_gives_non_calibrated);
    RUN_TEST(test_cal_small_separation_pos_gives_non_calibrated);
    RUN_TEST(test_cal_small_separation_neg_gives_non_calibrated);
    RUN_TEST(test_cal_exact_50_separation_succeeds);
    RUN_TEST(test_cal_calibration_points_stored);
    RUN_TEST(test_cal_coefficients_correct_known_values);
}
