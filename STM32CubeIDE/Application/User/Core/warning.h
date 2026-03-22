/**
 * @file warning.h
 * @brief PPO2 warning subsystem for sensor monitoring
 *
 * Provides visual (color-coded) and future vibration warnings based on:
 * - Sensor drift from active setpoint
 * - Proximity to min/max safety limits
 * - Critical limit violations
 */

#ifndef WARNING_H
#define WARNING_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/**
 * @brief Warning severity levels (priority order: highest to lowest)
 */
typedef enum {
    WARNING_NONE = 0,          // Green border, white text, black panel
    WARNING_YELLOW_DRIFT,      // Yellow border, yellow text (setpoint drift)
    WARNING_NEAR_CRITICAL,     // Red border, red text (approaching limits)
    WARNING_CRITICAL           // Red border, red panel background, white text (limit exceeded)
} warning_level_t;

/**
 * @brief Check sensor warning level based on current PPO reading
 * @param sensor_index Sensor index (0-2 for SENSOR1-SENSOR3)
 * @return warning_level_t enum value
 * @note Returns WARNING_NONE if sensor is invalid (s_valid[i] == false)
 */
warning_level_t warning_check_sensor(uint8_t sensor_index);

/**
 * @brief Apply visual styling to sensor display based on warning level
 * @param sensor_index Sensor index (0-2)
 * @param level Warning level to apply
 * @param panel_obj LVGL panel object (border color and background for CRITICAL)
 * @param label_obj LVGL label object (text color, transparent background)
 * @note For CRITICAL level, red background is applied to panel, not label
 */
void warning_apply_style(uint8_t sensor_index, warning_level_t level,
                         lv_obj_t* panel_obj, lv_obj_t* label_obj);

/**
 * @brief Trigger vibration alert for the given warning level.
 *        Call once per 1 Hz screen update with the highest active level.
 *        Passing WARNING_NONE stops the motor and clears all state.
 * @param level Highest active warning level across all sensors
 */
void warning_trigger_vibration(warning_level_t level);

/**
 * @brief Advance the vibro state machine. Call every 5 ms from the scheduler.
 */
void vibro_tick_5ms(void);

/**
 * @brief Acknowledge (snooze) the active vibro warning.
 *        Stops the motor immediately and suppresses re-trigger for the current
 *        warning level until it changes. Call from BTN_M handler.
 */
void vibro_acknowledge(void);

/**
 * @brief Returns true while a vibro pattern is running (motor on or in a pause step).
 */
bool vibro_is_active(void);

#endif // WARNING_H
