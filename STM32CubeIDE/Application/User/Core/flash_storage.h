/**
 * @file flash_storage.h
 * @brief Flash storage for sensor calibration data persistence (public API).
 *
 * Implementation delegated to ext_flash_storage (W25Q128 external SPI NOR flash).
 * All public API signatures are preserved so callers need no changes.
 */

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor.h"
#include "calibration.h"
#include "flash_utils.h"
#include "ext_flash_storage.h"

/**
 * @brief Save current sensor_data calibration to the next free flash slot.
 * @return true on success, false on write failure.
 */
bool flash_calibration_save(void);

/**
 * @brief Load the newest valid calibration from flash into sensor_data.
 * @return true if a valid calibration was found and loaded, false otherwise.
 */
bool flash_calibration_load(void);

/**
 * @brief Erase the calibration storage (factory reset).
 * @note All stored calibrations are permanently lost.
 */
void flash_calibration_erase(void);

/**
 * @brief Count valid calibration entries.
 * @return Number of valid calibration entries stored.
 */
uint32_t flash_calibration_get_count(void);

/**
 * @brief Iterate through calibration entries from newest to oldest.
 * @param callback    Function called for each valid entry
 * @param user_data   Context pointer passed to callback
 * @param max_entries Maximum entries to process (0 = all)
 * @return Number of valid entries processed
 */
uint32_t flash_calibration_iterate(
    flash_calib_iterator_cb callback,
    void *user_data,
    uint32_t max_entries
);

#endif /* FLASH_STORAGE_H */
