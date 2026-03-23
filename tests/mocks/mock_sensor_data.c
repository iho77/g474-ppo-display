/**
 * @file mock_sensor_data.c
 * @brief Global sensor_data, active_setpoint, and htim5 definitions for tests
 *
 * In production, sensor_data is defined in sensor.c inside an #ifndef UNIT_TEST
 * guard. When UNIT_TEST is defined, sensor.c declares them as extern and we
 * provide them here.
 */

#include "mock_sensor_data.h"
#include <string.h>

/* ── Globals ─────────────────────────────────────────────────────────────────── */

volatile sensor_data_t sensor_data;
uint8_t active_setpoint = 1;   /* Default to setpoint 1 */

/* HAL TIM handle stub required by warning.c vibro helpers */
TIM_HandleTypeDef htim5;

/* ── Test helper ─────────────────────────────────────────────────────────────── */

void reset_sensor_data(void) {
    memset((void*)&sensor_data, 0, sizeof(sensor_data));

    /* Realistic default settings matching settings_init_defaults() */
    sensor_data.settings.lcd_brightness        = 100;
    sensor_data.settings.min_ppo_threshold     = 160;   /* mbar */
    sensor_data.settings.max_ppo_threshold     = 1400;  /* mbar */
    sensor_data.settings.delta_ppo_threshold   = 100;   /* mbar */
    sensor_data.settings.warning_ppo_threshold = 100;   /* mbar */
    sensor_data.settings.ppo_setpoint1         = 700;   /* mbar */
    sensor_data.settings.ppo_setpoint2         = 1100;  /* mbar */

    /* Mark sensors as valid and calibrated for warning tests */
    for (int i = 0; i < SENSOR_COUNT - 2; i++) {
        sensor_data.s_valid[i]            = true;
        sensor_data.os_s_cal_state[i]     = NON_CALIBRATED;
        sensor_data.o2_s_ppo[i]           = 700;  /* Nominal setpoint */
        sensor_data.s_cal[0][i]           = 0.0f; /* slope */
        sensor_data.s_cal[1][i]           = 0.0f; /* intercept */
    }

    /* Battery at full charge */
    sensor_data.battery_voltage_mv   = 3300;
    sensor_data.battery_percentage   = 100;
    sensor_data.battery_low          = false;

    /* Pressure at atmospheric */
    sensor_data.pressure.surface_pressure_mbar = 101325;
    sensor_data.pressure.pressure_mbar         = 101325;
    sensor_data.pressure.temperature_c         = 3000;
    sensor_data.pressure.depth_mm              = 0;
    sensor_data.pressure.pressure_valid        = true;

    /* Drift trackers zeroed (already done by memset) */

    active_setpoint = 1;
}
