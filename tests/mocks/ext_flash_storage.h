/**
 * @file ext_flash_storage.h
 * @brief External flash storage stub for native x86 unit testing
 *
 * settings_storage.c delegates to ext_settings_save/load/erase.
 * Those functions touch SPI hardware and are NOT compiled in tests.
 * This stub satisfies the #include in settings_storage.c.
 * The actual linker-level stubs (if needed) live in mock_sensor_data.c.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "settings_storage.h"

/* Weak stubs so the linker is satisfied without the real ext_flash_storage.c */
static inline bool ext_settings_save(const settings_data_t *s) { (void)s; return true; }
static inline bool ext_settings_load(settings_data_t *s)       { (void)s; return false; }
static inline void ext_settings_erase(void)                    {}
static inline bool ext_flash_storage_init(void *hspi)          { (void)hspi; return true; }

typedef void (*flash_calib_iterator_cb)(
    uint32_t sequence,
    const float slopes[3],
    const float intercepts[3],
    void *user_data
);

static inline bool     ext_cal_save(void)    { return false; }
static inline bool     ext_cal_load(void)    { return false; }
static inline void     ext_cal_erase(void)   {}
static inline uint32_t ext_cal_get_count(void) { return 0; }
static inline uint32_t ext_cal_iterate(flash_calib_iterator_cb cb, void *ud, uint32_t max) {
    (void)cb; (void)ud; (void)max; return 0;
}
