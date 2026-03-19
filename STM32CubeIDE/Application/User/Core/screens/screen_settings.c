/**
 * @file screen_settings.c
 * @brief Settings screen - User preferences configuration
 *
 * Configure system settings with state machine:
 * State 0: Navigation (BTN_M cycles, BTN_S enters edit)
 * States 1-7: Edit parameter (BTN_M increments, BTN_S confirms)
 * State 8: Save & Exit (BTN_S saves to flash)
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "sensor.h"
#include "settings_storage.h"
#include "button.h"
#include "stm32g4xx_hal.h"
#include "main.h"
#include <stdio.h>

// External references
extern volatile sensor_data_t sensor_data;
extern TIM_HandleTypeDef htim2;

// ============================================================================
// PRIVATE VARIABLES - Widget references and state machine
// ============================================================================

// State machine: 0=nav, 1-7=edit params, 8=save&exit
static uint8_t edit_state = 0;
static uint8_t selected_item = 0;  // 0-7 (7 parameters + save option)

// Working copy of settings (modified before saving)
static settings_data_t working_settings;

// Label pairs: [name_label, value_label] for each of 8 items
static lv_obj_t* lbl_names[8] = {NULL};
static lv_obj_t* lbl_values[8] = {NULL};

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {
    // Title
    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "SETTINGS");
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 5);

    // Create 8 rows (7 parameters + save option)
    const char* param_names[] = {
        "Brightness",
        "Min PPO",
        "Max PPO",
        "Delta PPO",
        "Warning PPO",
        "Setpoint 1",
        "Setpoint 2",
        "Save & Exit"
    };

    int16_t y_start = 30;
    int16_t y_spacing = 26;

    for (uint8_t i = 0; i < 8; i++) {
        // Name label (left)
        lbl_names[i] = lv_label_create(scr);
        lv_label_set_text(lbl_names[i], param_names[i]);
        lv_obj_set_style_text_color(lbl_names[i], lv_color_white(), 0);
        lv_obj_align(lbl_names[i], LV_ALIGN_TOP_LEFT, 10, (lv_coord_t)(y_start + (i * y_spacing)));

        // Value label (right) - skip for save option
        if (i < 7) {
            lbl_values[i] = lv_label_create(scr);
            lv_label_set_text(lbl_values[i], "---");
            lv_obj_set_style_text_color(lbl_values[i], lv_color_white(), 0);
            lv_obj_align(lbl_values[i], LV_ALIGN_TOP_RIGHT, -10, (lv_coord_t)(y_start + (i * y_spacing)));
        }
    }
}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Update value labels with current working settings
 */
static void update_value_labels(void) {
    static char buf[16];

    if (lbl_values[0]) {
        (void)snprintf(buf, sizeof(buf), "%d%%", working_settings.lcd_brightness);
        lv_label_set_text(lbl_values[0], buf);
    }
    if (lbl_values[1]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.min_ppo_threshold);
        lv_label_set_text(lbl_values[1], buf);
    }
    if (lbl_values[2]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.max_ppo_threshold);
        lv_label_set_text(lbl_values[2], buf);
    }
    if (lbl_values[3]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.delta_ppo_threshold);
        lv_label_set_text(lbl_values[3], buf);
    }
    if (lbl_values[4]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.warning_ppo_threshold);
        lv_label_set_text(lbl_values[4], buf);
    }
    if (lbl_values[5]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.ppo_setpoint1);
        lv_label_set_text(lbl_values[5], buf);
    }
    if (lbl_values[6]) {
        (void)snprintf(buf, sizeof(buf), "%d", working_settings.ppo_setpoint2);
        lv_label_set_text(lbl_values[6], buf);
    }
}

/**
 * @brief Update highlight colors based on state
 */
static void update_highlights(void) {
    for (uint8_t i = 0; i < 8; i++) {
        if (i == selected_item && edit_state == 0) {
            // Navigation mode - selected item in cyan
            lv_obj_set_style_text_color(lbl_names[i], lv_color_make(0, 255, 255), 0);
            if (i < 7 && lbl_values[i]) {
                lv_obj_set_style_text_color(lbl_values[i], lv_color_make(0, 255, 255), 0);
            }
        } else if (i == selected_item && edit_state > 0) {
            // Edit mode - editing item in yellow
            lv_obj_set_style_text_color(lbl_names[i], lv_color_make(255, 255, 0), 0);
            if (i < 7 && lbl_values[i]) {
                lv_obj_set_style_text_color(lbl_values[i], lv_color_make(255, 255, 0), 0);
            }
        } else {
            // Normal white
            lv_obj_set_style_text_color(lbl_names[i], lv_color_white(), 0);
            if (i < 7 && lbl_values[i]) {
                lv_obj_set_style_text_color(lbl_values[i], lv_color_white(), 0);
            }
        }
    }
}

/**
 * @brief Increment parameter with wraparound
 */
static void increment_parameter(uint8_t param_index) {
    switch (param_index) {
        case 0: // Brightness
            working_settings.lcd_brightness += 10;
            if (working_settings.lcd_brightness > BRIGHTNESS_MAX) {
                working_settings.lcd_brightness = BRIGHTNESS_MIN;
            }
            break;
        case 1: // Min PPO
            working_settings.min_ppo_threshold += 10;
            if (working_settings.min_ppo_threshold > MIN_PPO_MAX) {
                working_settings.min_ppo_threshold = MIN_PPO_MIN;
            }
            break;
        case 2: // Max PPO
            working_settings.max_ppo_threshold += 10;
            if (working_settings.max_ppo_threshold > MAX_PPO_MAX) {
                working_settings.max_ppo_threshold = MAX_PPO_MIN;
            }
            break;
        case 3: // Delta PPO
            working_settings.delta_ppo_threshold += 10;
            if (working_settings.delta_ppo_threshold > DELTA_PPO_MAX) {
                working_settings.delta_ppo_threshold = DELTA_PPO_MIN;
            }
            break;
        case 4: // Warning PPO
            working_settings.warning_ppo_threshold += 10;
            if (working_settings.warning_ppo_threshold > WARNING_PPO_MAX) {
                working_settings.warning_ppo_threshold = WARNING_PPO_MIN;
            }
            break;
        case 5: // Setpoint 1
            working_settings.ppo_setpoint1 += 10;
            if (working_settings.ppo_setpoint1 > SETPOINT1_MAX) {
                working_settings.ppo_setpoint1 = SETPOINT1_MIN;
            }
            break;
        case 6: // Setpoint 2
            working_settings.ppo_setpoint2 += 10;
            if (working_settings.ppo_setpoint2 > SETPOINT2_MAX) {
                working_settings.ppo_setpoint2 = SETPOINT2_MIN;
            }
            break;
        default:
            break;
    }
}

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the settings screen
 */
lv_obj_t* screen_settings_create(void) {
    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Initialize state machine
    edit_state = 0;
    selected_item = 0;

    // Load current settings into working copy
    working_settings = sensor_data.settings;

    // Build screen content
    build_screen_content(scr);

    // Initial update
    update_value_labels();
    update_highlights();

    return scr;
}

/**
 * @brief Custom button handler - implements state machine for navigation and editing
 * @param evt Button event (BTN_M or BTN_S)
 *
 * State machine:
 * - State 0 (Navigation): BTN_M cycles items, BTN_S enters edit mode
 * - States 1-7 (Edit): BTN_M increments value, BTN_S confirms and returns to nav
 * - State 8 (Save): BTN_S saves to flash and exits to menu
 */
void screen_settings_on_button(btn_event_t evt) {
    if (edit_state == 0) {
        // Navigation mode
        if (evt == BTN_M_PRESS) {
            // Cycle selected item
            selected_item++;
            if (selected_item > 7) {
                selected_item = 0;
            }
            update_highlights();
        } else if (evt == BTN_S_PRESS) {
            if (selected_item == 7) {
                // Save & Exit selected
                settings_validate(&working_settings);
                if (settings_save(&working_settings)) {
                    // Update global settings
                    sensor_data.settings = working_settings;
                    // Apply brightness immediately via PWM (TIM2 CH1) with gamma correction
                    uint8_t brightness = working_settings.lcd_brightness;
                    uint8_t gamma_index = brightness / 10;
                    if (gamma_index > 10) gamma_index = 10;
                    uint16_t pwm_value = backlight_gamma_lut[gamma_index];
                    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pwm_value);
                }
                // Return to menu
                screen_manager_switch(SCREEN_MENU);
            } else {
                // Enter edit mode for selected parameter
                edit_state = selected_item + 1;
                update_highlights();
            }
        }
    } else {
        // Edit mode (states 1-7)
        if (evt == BTN_M_PRESS) {
            // Increment parameter value
            increment_parameter(selected_item);
            update_value_labels();

            // Apply brightness immediately in real-time with gamma correction
            if (selected_item == 0) {
                uint8_t brightness = working_settings.lcd_brightness;
                uint8_t gamma_index = brightness / 10;
                if (gamma_index > 10) gamma_index = 10;
                uint16_t pwm_value = backlight_gamma_lut[gamma_index];
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pwm_value);
            }
        } else if (evt == BTN_S_PRESS) {
            // Confirm and return to navigation
            edit_state = 0;
            update_highlights();
        }
    }
}
