/**
 * @file flash_utils.c
 * @brief Shared flash storage utilities implementation
 */

#include "flash_utils.h"
#include "stm32g4xx_hal.h"

// ============================================================================
// CRC-16/CCITT
// ============================================================================
uint16_t flash_crc16_ccitt(const uint8_t* data, uint32_t length) {
    uint16_t crc = 0xFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        }
    }
    return crc;
}

// ============================================================================
// CRC-32 (ISO 3309, reflected, poly 0xEDB88320)
// ============================================================================
uint32_t flash_crc32(const uint8_t *data, uint32_t length) {
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320UL) : (crc >> 1U);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

// ============================================================================
// uint64_t packing helpers
// ============================================================================
uint64_t flash_make_dword(uint32_t low, uint32_t high) {
    return ((uint64_t)high << 32) | (uint64_t)low;
}

uint32_t flash_get_low_word(uint64_t dword) {
    return (uint32_t)(dword & 0xFFFFFFFF);
}

uint32_t flash_get_high_word(uint64_t dword) {
    return (uint32_t)(dword >> 32);
}

// ============================================================================
// Float <-> uint32_t conversion
// ============================================================================
typedef union {
    float f;
    uint32_t u;
} float_uint32_t;

uint32_t flash_float_to_u32(float val) {
    float_uint32_t conv;
    conv.f = val;
    return conv.u;
}

float flash_u32_to_float(uint32_t val) {
    float_uint32_t conv;
    conv.u = val;
    return conv.f;
}

// ============================================================================
// Flash operations
// ============================================================================
void flash_unlock(void) {
    HAL_FLASH_Unlock();
}

void flash_lock(void) {
    HAL_FLASH_Lock();
}

void flash_clear_errors(void) {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
}

bool flash_page_erase(uint32_t page_number) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.Banks = FLASH_BANK_1;
    erase_init.Page = page_number;
    erase_init.NbPages = 1;

    flash_unlock();
    flash_clear_errors();
    HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    flash_lock();

    return (status == HAL_OK);
}
