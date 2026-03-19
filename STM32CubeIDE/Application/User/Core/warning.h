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
 * @brief Trigger vibration alert (STUB - future implementation)
 * @param level Warning level that triggered the alert
 * @note This is a placeholder for future hardware integration
 */
void warning_trigger_vibration(warning_level_t level);

#endif // WARNING_H
