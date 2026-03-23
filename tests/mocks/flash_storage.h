/**
 * @file flash_storage.h
 * @brief Internal flash storage stub for native x86 unit testing
 *
 * sensor_diagnostics.c includes flash_storage.h and calls
 * flash_calibration_get_count() / flash_calibration_iterate().
 * Both are HAL/SPI-dependent; we stub them to return empty results.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef void (*flash_calib_iterator_cb)(
    uint32_t sequence,
    const float slopes[3],
    const float intercepts[3],
    void *user_data
);

static inline uint32_t flash_calibration_get_count(void) { return 0; }
static inline uint32_t flash_calibration_iterate(
        flash_calib_iterator_cb cb, void *ud, uint32_t max) {
    (void)cb; (void)ud; (void)max; return 0;
}
