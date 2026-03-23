/**
 * @file test_settings.c
 * @brief Unit tests for settings_storage.c — pure logic functions only
 *
 * settings_save(), settings_load(), settings_erase() delegate to
 * ext_flash_storage (SPI hardware) and are NOT under test here.
 *
 * Tested functions:
 *  settings_init_defaults() — populates struct with correct default values
 *  settings_validate()      — clamps values to min/max constraints
 *
 * Scenarios:
 *  1.  init_defaults → all fields equal documented defaults
 *  2.  validate → values within range → unchanged
 *  3.  validate → lcd_brightness below min → clamped to BRIGHTNESS_MIN
 *  4.  validate → lcd_brightness above max → clamped to BRIGHTNESS_MAX
 *  5.  validate → min_ppo below MIN_PPO_MIN → clamped
 *  6.  validate → min_ppo above MIN_PPO_MAX → clamped
 *  7.  validate → max_ppo below MAX_PPO_MIN → clamped
 *  8.  validate → max_ppo above MAX_PPO_MAX → clamped
 *  9.  validate → delta_ppo below min → clamped
 * 10.  validate → delta_ppo above max → clamped
 * 11.  validate → warning_ppo below min → clamped
 * 12.  validate → warning_ppo above max → clamped
 * 13.  validate → setpoint1 below min → clamped
 * 14.  validate → setpoint1 above max → clamped
 * 15.  validate → setpoint2 below min → clamped
 * 16.  validate → setpoint2 above max → clamped
 */

#include <assert.h>
#include <stdio.h>
#include "settings_storage.h"

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

/* ── Tests: settings_init_defaults ───────────────────────────────────────── */

static void test_settings_defaults_brightness(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.lcd_brightness == BRIGHTNESS_DEFAULT);
}

static void test_settings_defaults_min_ppo(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.min_ppo_threshold == MIN_PPO_DEFAULT);
}

static void test_settings_defaults_max_ppo(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.max_ppo_threshold == MAX_PPO_DEFAULT);
}

static void test_settings_defaults_delta_ppo(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.delta_ppo_threshold == DELTA_PPO_DEFAULT);
}

static void test_settings_defaults_warning_ppo(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.warning_ppo_threshold == WARNING_PPO_DEFAULT);
}

static void test_settings_defaults_setpoints(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    assert(s.ppo_setpoint1 == SETPOINT1_DEFAULT);
    assert(s.ppo_setpoint2 == SETPOINT2_DEFAULT);
}

/* ── Tests: settings_validate — in-range values unchanged ────────────────── */

static void test_settings_validate_in_range_unchanged(void) {
    settings_data_t s;
    settings_init_defaults(&s);  /* Start with valid defaults */
    settings_validate(&s);

    assert(s.lcd_brightness         == BRIGHTNESS_DEFAULT);
    assert(s.min_ppo_threshold      == MIN_PPO_DEFAULT);
    assert(s.max_ppo_threshold      == MAX_PPO_DEFAULT);
    assert(s.delta_ppo_threshold    == DELTA_PPO_DEFAULT);
    assert(s.warning_ppo_threshold  == WARNING_PPO_DEFAULT);
    assert(s.ppo_setpoint1          == SETPOINT1_DEFAULT);
    assert(s.ppo_setpoint2          == SETPOINT2_DEFAULT);
}

/* ── Tests: settings_validate — brightness clamping ─────────────────────── */

static void test_settings_validate_brightness_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.lcd_brightness = BRIGHTNESS_MIN - 1;
    settings_validate(&s);
    assert(s.lcd_brightness == BRIGHTNESS_MIN);
}

static void test_settings_validate_brightness_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.lcd_brightness = BRIGHTNESS_MAX + 1;
    settings_validate(&s);
    assert(s.lcd_brightness == BRIGHTNESS_MAX);
}

static void test_settings_validate_brightness_at_min_unchanged(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.lcd_brightness = BRIGHTNESS_MIN;
    settings_validate(&s);
    assert(s.lcd_brightness == BRIGHTNESS_MIN);
}

/* ── Tests: settings_validate — min_ppo clamping ────────────────────────── */

static void test_settings_validate_min_ppo_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.min_ppo_threshold = MIN_PPO_MIN - 1;
    settings_validate(&s);
    assert(s.min_ppo_threshold == MIN_PPO_MIN);
}

static void test_settings_validate_min_ppo_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.min_ppo_threshold = MIN_PPO_MAX + 1;
    settings_validate(&s);
    assert(s.min_ppo_threshold == MIN_PPO_MAX);
}

/* ── Tests: settings_validate — max_ppo clamping ────────────────────────── */

static void test_settings_validate_max_ppo_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.max_ppo_threshold = MAX_PPO_MIN - 1;
    settings_validate(&s);
    assert(s.max_ppo_threshold == MAX_PPO_MIN);
}

static void test_settings_validate_max_ppo_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.max_ppo_threshold = MAX_PPO_MAX + 1;
    settings_validate(&s);
    assert(s.max_ppo_threshold == MAX_PPO_MAX);
}

/* ── Tests: settings_validate — delta_ppo clamping ──────────────────────── */

static void test_settings_validate_delta_ppo_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.delta_ppo_threshold = DELTA_PPO_MIN - 1;
    settings_validate(&s);
    assert(s.delta_ppo_threshold == DELTA_PPO_MIN);
}

static void test_settings_validate_delta_ppo_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.delta_ppo_threshold = DELTA_PPO_MAX + 1;
    settings_validate(&s);
    assert(s.delta_ppo_threshold == DELTA_PPO_MAX);
}

/* ── Tests: settings_validate — warning_ppo clamping ────────────────────── */

static void test_settings_validate_warning_ppo_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.warning_ppo_threshold = WARNING_PPO_MIN - 1;
    settings_validate(&s);
    assert(s.warning_ppo_threshold == WARNING_PPO_MIN);
}

static void test_settings_validate_warning_ppo_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.warning_ppo_threshold = WARNING_PPO_MAX + 1;
    settings_validate(&s);
    assert(s.warning_ppo_threshold == WARNING_PPO_MAX);
}

/* ── Tests: settings_validate — setpoint clamping ───────────────────────── */

static void test_settings_validate_setpoint1_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.ppo_setpoint1 = SETPOINT1_MIN - 1;
    settings_validate(&s);
    assert(s.ppo_setpoint1 == SETPOINT1_MIN);
}

static void test_settings_validate_setpoint1_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.ppo_setpoint1 = SETPOINT1_MAX + 1;
    settings_validate(&s);
    assert(s.ppo_setpoint1 == SETPOINT1_MAX);
}

static void test_settings_validate_setpoint2_below_min(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.ppo_setpoint2 = SETPOINT2_MIN - 1;
    settings_validate(&s);
    assert(s.ppo_setpoint2 == SETPOINT2_MIN);
}

static void test_settings_validate_setpoint2_above_max(void) {
    settings_data_t s;
    settings_init_defaults(&s);
    s.ppo_setpoint2 = SETPOINT2_MAX + 1;
    settings_validate(&s);
    assert(s.ppo_setpoint2 == SETPOINT2_MAX);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_settings_tests(void) {
    printf("\n[SETTINGS TESTS - settings_storage.c (pure logic only)]\n");
    /* init_defaults */
    RUN_TEST(test_settings_defaults_brightness);
    RUN_TEST(test_settings_defaults_min_ppo);
    RUN_TEST(test_settings_defaults_max_ppo);
    RUN_TEST(test_settings_defaults_delta_ppo);
    RUN_TEST(test_settings_defaults_warning_ppo);
    RUN_TEST(test_settings_defaults_setpoints);
    /* validate — in-range */
    RUN_TEST(test_settings_validate_in_range_unchanged);
    /* validate — brightness */
    RUN_TEST(test_settings_validate_brightness_below_min);
    RUN_TEST(test_settings_validate_brightness_above_max);
    RUN_TEST(test_settings_validate_brightness_at_min_unchanged);
    /* validate — min_ppo */
    RUN_TEST(test_settings_validate_min_ppo_below_min);
    RUN_TEST(test_settings_validate_min_ppo_above_max);
    /* validate — max_ppo */
    RUN_TEST(test_settings_validate_max_ppo_below_min);
    RUN_TEST(test_settings_validate_max_ppo_above_max);
    /* validate — delta_ppo */
    RUN_TEST(test_settings_validate_delta_ppo_below_min);
    RUN_TEST(test_settings_validate_delta_ppo_above_max);
    /* validate — warning_ppo */
    RUN_TEST(test_settings_validate_warning_ppo_below_min);
    RUN_TEST(test_settings_validate_warning_ppo_above_max);
    /* validate — setpoints */
    RUN_TEST(test_settings_validate_setpoint1_below_min);
    RUN_TEST(test_settings_validate_setpoint1_above_max);
    RUN_TEST(test_settings_validate_setpoint2_below_min);
    RUN_TEST(test_settings_validate_setpoint2_above_max);
}
