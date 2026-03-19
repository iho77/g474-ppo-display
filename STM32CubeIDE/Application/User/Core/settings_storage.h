/**
 * @file settings_storage.h
 * @brief Flash storage for system setup parameters persistence
 *
 * All I/O delegated to W25Q128 external SPI NOR flash via ext_flash_storage.
 * Implements CRC16-CCITT validation and wear leveling across 85 slots.
 */

#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "flash_utils.h"

#define SETTINGS_MAGIC              0x5E771465UL  // "SETTINGS"
#define SETTINGS_SLOTS              85            // 2048 / 24 = 85 slots per page (8 bytes unused)

/**
 * @brief Flash storage layout for settings - explicit uint64_t array
 *
 * Layout (24 bytes = 3 doublewords):
 *   DW0: magic (32-bit low) | sequence (32-bit high)
 *   DW1: crc16 (16-bit) | brightness (8-bit) | reserved (8-bit) |
 *        min_ppo (16-bit) | max_ppo (16-bit)
 *   DW2: delta_ppo (16-bit) | warning_ppo (16-bit) |
 *        setpoint1 (16-bit) | setpoint2 (16-bit)
 */
#define SETTINGS_ENTRY_DWORDS  3   // 24 bytes total
#define SETTINGS_ENTRY_SIZE    (SETTINGS_ENTRY_DWORDS * 8U)

// Parameter constraints
#define BRIGHTNESS_DEFAULT      100
#define BRIGHTNESS_MIN          10
#define BRIGHTNESS_MAX          100

#define MIN_PPO_DEFAULT         160   // mbar (0.16 bar)
#define MIN_PPO_MIN             100
#define MIN_PPO_MAX             300

#define MAX_PPO_DEFAULT         1400  // mbar (1.4 bar)
#define MAX_PPO_MIN             1000
#define MAX_PPO_MAX             1600

#define DELTA_PPO_DEFAULT       100   // mbar
#define DELTA_PPO_MIN           50
#define DELTA_PPO_MAX           200

#define WARNING_PPO_DEFAULT     100   // mbar - distance from limits/setpoint for warnings
#define WARNING_PPO_MIN         100
#define WARNING_PPO_MAX         300

#define SETPOINT1_DEFAULT       700   // mbar (0.7 bar)
#define SETPOINT1_MIN           500
#define SETPOINT1_MAX           1000

#define SETPOINT2_DEFAULT       1100  // mbar (1.1 bar)
#define SETPOINT2_MIN           800
#define SETPOINT2_MAX           1300

/**
 * @brief System settings structure (7 parameters)
 */
typedef struct {
    uint8_t lcd_brightness;      // 0-100 %
    uint16_t min_ppo_threshold;  // mbar (100-300)
    uint16_t max_ppo_threshold;  // mbar (1000-1600)
    uint16_t delta_ppo_threshold; // mbar (50-200)
    uint16_t warning_ppo_threshold; // mbar (100-300) - warning distance
    uint16_t ppo_setpoint1;      // mbar (500-1000)
    uint16_t ppo_setpoint2;      // mbar (800-1300)
} settings_data_t;

/**
 * @brief Initialize settings with default values
 * @param settings Pointer to settings structure to initialize
 */
void settings_init_defaults(settings_data_t* settings);

/**
 * @brief Validate and clamp settings values to constraints
 * @param settings Pointer to settings structure to validate
 */
void settings_validate(settings_data_t* settings);

/**
 * @brief Save settings to next flash slot
 * @param settings Pointer to settings structure to save
 * @return true if save successful, false if flash operation failed
 * @note Automatically handles circular buffer wraparound and page erase
 */
bool settings_save(const settings_data_t* settings);

/**
 * @brief Load newest valid settings from flash
 * @param settings Pointer to settings structure to load into
 * @return true if valid settings found and loaded, false if no settings
 * @note Scans all 85 slots to find highest valid sequence number
 */
bool settings_load(settings_data_t* settings);

/**
 * @brief Factory reset - erase entire settings page
 * @note Use for settings reset - all stored settings will be lost
 */
void settings_erase(void);

#endif // SETTINGS_STORAGE_H
