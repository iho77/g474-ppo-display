/**
 * @file test_warning.c
 * @brief Unit tests for warning.c — warning_check_sensor() only
 *
 * warning_apply_style() is LVGL-dependent (excluded).
 * warning_trigger_vibration() / vibro_tick_5ms() call HAL TIM — those
 * are compiled in (the HAL stubs make them no-ops) but behavioral
 * testing of motor timing is out of scope for a pure-logic suite.
 *
 * Tested scenarios for warning_check_sensor():
 *  1.  Invalid sensor index → WARNING_NONE
 *  2.  s_valid == false → WARNING_NONE
 *  3.  PPO within safe zone → WARNING_NONE
 *  4.  PPO below min_ppo → WARNING_CRITICAL
 *  5.  PPO above max_ppo → WARNING_CRITICAL
 *  6.  PPO exactly at min_ppo → WARNING_CRITICAL (not <=, strictly <)
 *      Wait — code says ppo < min_ppo so ppo==min_ppo is NOT critical
 *  7.  PPO within warning_dist of min → WARNING_NEAR_CRITICAL
 *  8.  PPO within warning_dist of max → WARNING_NEAR_CRITICAL
 *  9.  PPO drifted from setpoint > warning_dist → WARNING_YELLOW_DRIFT
 * 10.  PPO exactly at setpoint → WARNING_NONE
 * 11.  Priority: CRITICAL beats NEAR_CRITICAL (min exceeded)
 * 12.  Active setpoint 2 used when active_setpoint==2
 */

#include <assert.h>
#include <stdio.h>
#include "warning.h"
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

/* ── Helper: set sensor[0] PPO to given value, sensor valid ─────────────── */
static void set_ppo(int32_t ppo) {
    sensor_data.o2_s_ppo[0] = ppo;
    sensor_data.s_valid[0]   = true;
}

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_warn_invalid_sensor_index(void) {
    reset_sensor_data();
    /* SENSOR_COUNT-2 = 3, so index >= 3 is invalid */
    warning_level_t w = warning_check_sensor(SENSOR_COUNT - 2);
    assert(w == WARNING_NONE);
}

static void test_warn_invalid_sensor_s_valid_false(void) {
    reset_sensor_data();
    sensor_data.s_valid[0] = false;
    sensor_data.o2_s_ppo[0] = 700;
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NONE);
}

static void test_warn_normal_ppo_gives_none(void) {
    reset_sensor_data();
    /* defaults: min=160, max=1400, warning_dist=100, setpoint1=700 */
    set_ppo(700);  /* Exactly at setpoint, well within bounds */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NONE);
}

static void test_warn_ppo_below_min_critical(void) {
    reset_sensor_data();
    set_ppo(159);  /* Below min_ppo=160 */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_CRITICAL);
}

static void test_warn_ppo_above_max_critical(void) {
    reset_sensor_data();
    set_ppo(1401);  /* Above max_ppo=1400 */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_CRITICAL);
}

static void test_warn_ppo_exactly_at_min_not_critical(void) {
    reset_sensor_data();
    set_ppo(160);  /* Exactly at min — code uses strict < */
    warning_level_t w = warning_check_sensor(0);
    assert(w != WARNING_CRITICAL);
}

static void test_warn_ppo_exactly_at_max_not_critical(void) {
    reset_sensor_data();
    set_ppo(1400);  /* Exactly at max — code uses strict > */
    warning_level_t w = warning_check_sensor(0);
    assert(w != WARNING_CRITICAL);
}

static void test_warn_ppo_near_min_gives_near_critical(void) {
    reset_sensor_data();
    /* min=160, warning_dist=100 → near_critical zone: 160 ≤ ppo ≤ 260 */
    set_ppo(220);
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NEAR_CRITICAL);
}

static void test_warn_ppo_near_max_gives_near_critical(void) {
    reset_sensor_data();
    /* max=1400, warning_dist=100 → near_critical zone: 1300 ≤ ppo ≤ 1400 */
    set_ppo(1350);
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NEAR_CRITICAL);
}

static void test_warn_ppo_drifted_from_setpoint_gives_yellow(void) {
    reset_sensor_data();
    /* setpoint1=700, warning_dist=100 → drift zone: |ppo-700| > 100 */
    set_ppo(810);  /* 110 above setpoint */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_YELLOW_DRIFT);
}

static void test_warn_ppo_below_setpoint_drifted_gives_yellow(void) {
    reset_sensor_data();
    set_ppo(590);  /* 110 below setpoint */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_YELLOW_DRIFT);
}

static void test_warn_ppo_exactly_at_setpoint_gives_none(void) {
    reset_sensor_data();
    set_ppo(700);
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NONE);
}

static void test_warn_ppo_within_warning_dist_gives_none(void) {
    reset_sensor_data();
    /* setpoint=700, warning_dist=100 → ppo=790 is 90 away → NONE */
    set_ppo(790);
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_NONE);
}

static void test_warn_active_setpoint2_used(void) {
    reset_sensor_data();
    active_setpoint = 2;  /* Switch to setpoint 2 (1100 mbar) */
    /* setpoint2=1100, warning_dist=100 → drift zone: |ppo-1100| > 100 */
    /* ppo=950 → |950-1100|=150 > 100 → YELLOW (also not near limits) */
    set_ppo(950);
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_YELLOW_DRIFT);
    active_setpoint = 1;  /* Restore */
}

static void test_warn_priority_critical_over_near(void) {
    reset_sensor_data();
    /* ppo < min → CRITICAL regardless of near-critical zone */
    set_ppo(50);   /* Way below min=160 */
    warning_level_t w = warning_check_sensor(0);
    assert(w == WARNING_CRITICAL);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_warning_tests(void) {
    printf("\n[WARNING TESTS - warning.c (warning_check_sensor only)]\n");
    RUN_TEST(test_warn_invalid_sensor_index);
    RUN_TEST(test_warn_invalid_sensor_s_valid_false);
    RUN_TEST(test_warn_normal_ppo_gives_none);
    RUN_TEST(test_warn_ppo_below_min_critical);
    RUN_TEST(test_warn_ppo_above_max_critical);
    RUN_TEST(test_warn_ppo_exactly_at_min_not_critical);
    RUN_TEST(test_warn_ppo_exactly_at_max_not_critical);
    RUN_TEST(test_warn_ppo_near_min_gives_near_critical);
    RUN_TEST(test_warn_ppo_near_max_gives_near_critical);
    RUN_TEST(test_warn_ppo_drifted_from_setpoint_gives_yellow);
    RUN_TEST(test_warn_ppo_below_setpoint_drifted_gives_yellow);
    RUN_TEST(test_warn_ppo_exactly_at_setpoint_gives_none);
    RUN_TEST(test_warn_ppo_within_warning_dist_gives_none);
    RUN_TEST(test_warn_active_setpoint2_used);
    RUN_TEST(test_warn_priority_critical_over_near);
}
