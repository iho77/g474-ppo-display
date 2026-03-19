/**
 * @file warning.c
 * @brief PPO2 warning subsystem implementation
 */

#include "warning.h"
#include "sensor.h"
#include <stdlib.h>  // For abs()

warning_level_t warning_check_sensor(uint8_t sensor_index) {
    // 1. Check sensor validity
    if (sensor_index >= (SENSOR_COUNT - 2) || !sensor_data.s_valid[sensor_index]) {
        return WARNING_NONE;
    }

    // 2. Get sensor PPO reading (mbar)
    int32_t ppo = sensor_data.o2_s_ppo[sensor_index];

    // 3. Get thresholds from settings
    uint16_t min_ppo = sensor_data.settings.min_ppo_threshold;
    uint16_t max_ppo = sensor_data.settings.max_ppo_threshold;
    uint16_t warning_dist = sensor_data.settings.warning_ppo_threshold;

    // 4. Get active setpoint value
    uint16_t setpoint_value;
    if (active_setpoint == 1) {
        setpoint_value = sensor_data.settings.ppo_setpoint1;
    } else {
        setpoint_value = sensor_data.settings.ppo_setpoint2;
    }

    // 5. Check warning levels (PRIORITY ORDER - highest first)

    // CRITICAL: Exceeded min/max limits
    if (ppo < min_ppo || ppo > max_ppo) {
        return WARNING_CRITICAL;
    }

    // NEAR_CRITICAL: Within warning distance of limits
    // Cast to int32_t to prevent unsigned overflow/underflow
    if (ppo <= ((int32_t)min_ppo + (int32_t)warning_dist) ||
        ppo >= ((int32_t)max_ppo - (int32_t)warning_dist)) {
        return WARNING_NEAR_CRITICAL;
    }

    // YELLOW_DRIFT: Drifted from active setpoint
    int32_t drift = ppo - setpoint_value;
    if (drift < 0) drift = -drift;  // Absolute value
    if (drift > warning_dist) {
        return WARNING_YELLOW_DRIFT;
    }

    // NONE: All good
    return WARNING_NONE;
}

void warning_apply_style(uint8_t sensor_index, warning_level_t level,
                         lv_obj_t* panel_obj, lv_obj_t* label_obj) {
    uint32_t border_color, text_color, panel_bg_color, label_bg_color;
    lv_opa_t panel_bg_opa, label_bg_opa;

    // Suppress unused parameter warning
    (void)sensor_index;

    switch (level) {
        case WARNING_CRITICAL:
            border_color = 0xFF0000;      // Red border
            text_color = 0xFFFFFF;        // White text
            panel_bg_color = 0xFF0000;    // Red panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background (transparent)
            label_bg_opa = LV_OPA_TRANSP; // Transparent to show panel background
            break;

        case WARNING_NEAR_CRITICAL:
            border_color = 0xFF0000;      // Red border
            text_color = 0xFF0000;        // Red text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;

        case WARNING_YELLOW_DRIFT:
            border_color = 0xFFFF00;      // Yellow border
            text_color = 0xFFFF00;        // Yellow text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;

        case WARNING_NONE:
        default:
            border_color = 0x00FF00;      // Green border
            text_color = 0xFFFFFF;        // White text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;
    }

    // Apply border color and background to panel
    if (panel_obj) {
        lv_obj_set_style_border_color(panel_obj, lv_color_hex(border_color),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel_obj, lv_color_hex(panel_bg_color),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel_obj, panel_bg_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Apply text color and background to label
    if (label_obj) {
        lv_obj_set_style_text_color(label_obj, lv_color_hex(text_color),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(label_obj, lv_color_hex(label_bg_color),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label_obj, label_bg_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void warning_trigger_vibration(warning_level_t level) {
    // TODO: Future implementation - vibration motor control
    // Example: GPIO toggle, PWM pattern generation, etc.
    // Current: No-op stub
    (void)level;  // Suppress unused parameter warning
}
