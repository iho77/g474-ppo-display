/**
 * @file settings_storage.c
 * @brief Flash storage delegation layer — system settings persistence.
 *
 * All save/load/erase operations delegated to ext_flash_storage
 * (W25Q128 external SPI NOR flash). The internal STM32 flash page 62 is
 * no longer used for settings.
 *
 * settings_init_defaults() and settings_validate() are pure logic and remain here.
 */

#include "settings_storage.h"
#include "ext_flash_storage.h"

// ============================================================================
// PURE LOGIC — no flash I/O
// ============================================================================

void settings_init_defaults(settings_data_t* settings) {
    settings->lcd_brightness = BRIGHTNESS_DEFAULT;
    settings->min_ppo_threshold = MIN_PPO_DEFAULT;
    settings->max_ppo_threshold = MAX_PPO_DEFAULT;
    settings->delta_ppo_threshold = DELTA_PPO_DEFAULT;
    settings->warning_ppo_threshold = WARNING_PPO_DEFAULT;
    settings->ppo_setpoint1 = SETPOINT1_DEFAULT;
    settings->ppo_setpoint2 = SETPOINT2_DEFAULT;
}

void settings_validate(settings_data_t* settings) {
    if (settings->lcd_brightness < BRIGHTNESS_MIN) {
        settings->lcd_brightness = BRIGHTNESS_MIN;
    } else if (settings->lcd_brightness > BRIGHTNESS_MAX) {
        settings->lcd_brightness = BRIGHTNESS_MAX;
    }

    if (settings->min_ppo_threshold < MIN_PPO_MIN) {
        settings->min_ppo_threshold = MIN_PPO_MIN;
    } else if (settings->min_ppo_threshold > MIN_PPO_MAX) {
        settings->min_ppo_threshold = MIN_PPO_MAX;
    }

    if (settings->max_ppo_threshold < MAX_PPO_MIN) {
        settings->max_ppo_threshold = MAX_PPO_MIN;
    } else if (settings->max_ppo_threshold > MAX_PPO_MAX) {
        settings->max_ppo_threshold = MAX_PPO_MAX;
    }

    if (settings->delta_ppo_threshold < DELTA_PPO_MIN) {
        settings->delta_ppo_threshold = DELTA_PPO_MIN;
    } else if (settings->delta_ppo_threshold > DELTA_PPO_MAX) {
        settings->delta_ppo_threshold = DELTA_PPO_MAX;
    }

    if (settings->warning_ppo_threshold < WARNING_PPO_MIN) {
        settings->warning_ppo_threshold = WARNING_PPO_MIN;
    } else if (settings->warning_ppo_threshold > WARNING_PPO_MAX) {
        settings->warning_ppo_threshold = WARNING_PPO_MAX;
    }

    if (settings->ppo_setpoint1 < SETPOINT1_MIN) {
        settings->ppo_setpoint1 = SETPOINT1_MIN;
    } else if (settings->ppo_setpoint1 > SETPOINT1_MAX) {
        settings->ppo_setpoint1 = SETPOINT1_MAX;
    }

    if (settings->ppo_setpoint2 < SETPOINT2_MIN) {
        settings->ppo_setpoint2 = SETPOINT2_MIN;
    } else if (settings->ppo_setpoint2 > SETPOINT2_MAX) {
        settings->ppo_setpoint2 = SETPOINT2_MAX;
    }
}

// ============================================================================
// DELEGATION
// ============================================================================

bool settings_save(const settings_data_t* settings) {
    return ext_settings_save(settings);
}

bool settings_load(settings_data_t* settings) {
    return ext_settings_load(settings);
}

void settings_erase(void) {
    ext_settings_erase();
}
