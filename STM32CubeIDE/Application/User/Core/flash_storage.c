/**
 * @file flash_storage.c
 * @brief Flash storage delegation layer — calibration persistence.
 *
 * All operations delegated to ext_flash_storage (W25Q128 external SPI NOR flash).
 * The internal STM32 flash pages 62–63 are no longer used for calibration.
 */

#include "flash_storage.h"

bool flash_calibration_save(void)
{
    return ext_cal_save();
}

bool flash_calibration_load(void)
{
    return ext_cal_load();
}

void flash_calibration_erase(void)
{
    ext_cal_erase();
}

uint32_t flash_calibration_get_count(void)
{
    return ext_cal_get_count();
}

uint32_t flash_calibration_iterate(
    flash_calib_iterator_cb callback,
    void *user_data,
    uint32_t max_entries
) {
    return ext_cal_iterate(callback, user_data, max_entries);
}
