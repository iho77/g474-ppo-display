/**
 * @file ext_flash_storage.h
 * @brief External W25Q128 flash storage for calibration and settings persistence.
 *
 * Address map (W25Q128 16MB SPI NOR flash):
 *   Sector 0  (0x000000–0x000FFF, 4KB):  Settings Bank A (Ping)
 *   Sector 1  (0x001000–0x001FFF, 4KB):  Settings Bank B (Pong)
 *   Sectors 2–17 (0x002000–0x011FFF, 64KB): Calibration append log
 *
 * Settings: 28-byte ping-pong entry with CRC32.  Power-fail safe.
 * Calibration: 48-byte append-only log, 5 entries per 256-byte page,
 *              1280 entries total before sector erase and wrap.
 *
 * All public API signatures match the existing flash_storage / settings_storage
 * interfaces so callers require no changes.
 */

#ifndef EXT_FLASH_STORAGE_H
#define EXT_FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"
#include "settings_storage.h"

/* ── Address map ── */
#define EXT_SETTINGS_BANK_A      0x000000UL  /* Sector 0: settings ping */
#define EXT_SETTINGS_BANK_B      0x001000UL  /* Sector 1: settings pong */
#define EXT_CAL_SECTOR_FIRST     0x002000UL  /* Sector 2: start of cal log */
#define EXT_CAL_SECTOR_COUNT     16U          /* Sectors 2–17 */
#define EXT_CAL_ZONE_SIZE        (EXT_CAL_SECTOR_COUNT * 4096U)  /* 64KB */

/* ── Settings entry (28 bytes) ── */
#define EXT_SETTINGS_MAGIC       0x5E771466UL  /* bumped from internal 0x5E771465 */
#define EXT_SETTINGS_ENTRY_SIZE  28U

/* ── Calibration entry (48 bytes) ── */
#define EXT_CAL_MAGIC            0xCA11B8EEUL  /* bumped from internal 0xCA11B8ED */
#define EXT_CAL_ENTRY_SIZE       48U
#define EXT_CAL_ENTRIES_PER_PAGE 5U            /* 5 × 48 = 240 < 256 */
#define EXT_CAL_TOTAL_ENTRIES    1280U         /* 256 pages × 5 entries */

/**
 * @brief Callback for historical calibration iteration.
 *        Matches the old flash_calib_iterator_cb signature.
 */
typedef void (*flash_calib_iterator_cb)(
    uint32_t sequence,
    const float slopes[3],
    const float intercepts[3],
    void *user_data
);

/* ── Initialisation ── */

/**
 * @brief Initialise external flash module: verify chip ID and scan calibration log
 *        to find write pointer.
 * @param hspi SPI handle for W25Q128 (hspi2).
 * @return true if chip responded and init succeeded, false otherwise (non-fatal).
 */
bool ext_flash_storage_init(SPI_HandleTypeDef *hspi);

/* ── Settings API ── */

bool ext_settings_save(const settings_data_t *settings);
bool ext_settings_load(settings_data_t *settings);
void ext_settings_erase(void);

/* ── Calibration API ── */

bool     ext_cal_save(void);
bool     ext_cal_load(void);
void     ext_cal_erase(void);
uint32_t ext_cal_get_count(void);
uint32_t ext_cal_iterate(flash_calib_iterator_cb cb, void *ud, uint32_t max);

#endif /* EXT_FLASH_STORAGE_H */
