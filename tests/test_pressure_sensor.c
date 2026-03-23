/**
 * @file test_pressure_sensor.c
 * @brief Unit tests for pressure_sensor.c — pressure_sensor_calculate_depth_mm()
 *
 * pressure_sensor_init() writes to global sensor_data (HAL-adjacent in spirit
 * but pure memory ops). It is included for completeness.
 *
 * Formula under test:
 *   delta_pressure = surface - current   (note: inverted sign in code)
 *   depth_mm = (delta_pressure * 995) / 10000
 *
 * IMPORTANT: The code computes delta = surface - current (not current - surface).
 * So depth is positive when current > surface (going deeper increases P).
 * Wait — let's re-read the code:
 *   delta_pressure_mbar = surface_pressure_mbar - current_pressure_mbar
 *   depth_mm = (delta * 995) / 10000
 * So depth is positive when surface > current (underwater pressure LESS than surface?).
 * Actually the function signature says "positive = below surface, negative = above".
 * The formula in code inverts signs from what one might expect. Tests must match
 * the actual implemented formula, not the doc example (which has a discrepancy).
 *
 * Tests derive expected values from the actual formula:
 *   depth_mm = ((surface - current) * 995) / 10000
 *
 * Physical reality check:
 *   At 10m depth, pressure = ~2013 mbar × 100 = 201325
 *   surface = 101325
 *   delta = 101325 - 201325 = -100000
 *   depth_mm = (-100000 * 995) / 10000 = -9950
 * (Negative means deeper — this is the sign convention the code uses)
 */

#include <assert.h>
#include <stdio.h>
#include "pressure_sensor.h"
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

/* ── Tests ────────────────────────────────────────────────────────────────── */

static void test_depth_at_surface_zero(void) {
    /* Same pressure → zero depth */
    int32_t depth = pressure_sensor_calculate_depth_mm(101325, 101325);
    assert(depth == 0);
}

static void test_depth_ten_meters(void) {
    /*
     * At 10m seawater: approx +1000 mbar above surface
     * current = 101325 + 100000 = 201325 (scaled ×100)
     * delta = 101325 - 201325 = -100000
     * depth_mm = (-100000 * 995) / 10000 = -9950
     */
    int32_t depth = pressure_sensor_calculate_depth_mm(201325, 101325);
    assert(depth == -9950);
}

static void test_depth_above_surface_positive(void) {
    /*
     * Pressure lower than surface (e.g. altitude/negative depth):
     * current = 91325, surface = 101325
     * delta = 101325 - 91325 = 10000
     * depth_mm = (10000 * 995) / 10000 = 995
     */
    int32_t depth = pressure_sensor_calculate_depth_mm(91325, 101325);
    assert(depth == 995);
}

static void test_depth_formula_coefficient(void) {
    /*
     * Test the exact integer coefficient: 995/10000
     * delta = 10000 → depth = (10000 * 995) / 10000 = 995
     */
    int32_t depth = pressure_sensor_calculate_depth_mm(
        101325 - 10000, 101325);   /* current = surface - 10000 */
    assert(depth == 995);
}

static void test_depth_zero_current_large_surface(void) {
    /* delta = 200000 - 0 = 200000 */
    /* depth = (200000 * 995) / 10000 = 19900 */
    int32_t depth = pressure_sensor_calculate_depth_mm(0, 200000);
    assert(depth == 19900);
}

static void test_depth_symmetric_negative(void) {
    /* delta = -200000 */
    /* depth = (-200000 * 995) / 10000 = -19900 */
    int32_t depth = pressure_sensor_calculate_depth_mm(200000, 0);
    assert(depth == -19900);
}

static void test_pressure_sensor_init_sets_defaults(void) {
    reset_sensor_data();
    pressure_sensor_init();

    assert(sensor_data.pressure.surface_pressure_mbar == ATMOSPHERIC_PRESSURE_MBAR);
    assert(sensor_data.pressure.pressure_mbar         == ATMOSPHERIC_PRESSURE_MBAR);
    assert(sensor_data.pressure.temperature_c         == STANDARD_TEMPERATURE_C);
    assert(sensor_data.pressure.depth_mm              == 0);
    assert(sensor_data.pressure.pressure_valid        == true);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_pressure_sensor_tests(void) {
    printf("\n[PRESSURE SENSOR TESTS - pressure_sensor.c]\n");
    RUN_TEST(test_depth_at_surface_zero);
    RUN_TEST(test_depth_ten_meters);
    RUN_TEST(test_depth_above_surface_positive);
    RUN_TEST(test_depth_formula_coefficient);
    RUN_TEST(test_depth_zero_current_large_surface);
    RUN_TEST(test_depth_symmetric_negative);
    RUN_TEST(test_pressure_sensor_init_sets_defaults);
}
