/**
 * @file flash_utils.h
 * @brief Shared flash storage utilities for STM32G4
 *
 * Common functions for CRC calculation, uint64_t packing, and flash operations
 * used by both calibration and settings storage modules.
 */

#ifndef FLASH_UTILS_H
#define FLASH_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// CRC-16/CCITT (polynomial 0x1021)
// ============================================================================
/**
 * @brief Calculate CRC-16/CCITT over a byte buffer
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @return 16-bit CRC value
 */
uint16_t flash_crc16_ccitt(const uint8_t* data, uint32_t length);

/**
 * @brief Calculate CRC-32 (ISO 3309, reflected, poly 0xEDB88320) over a byte buffer
 * @param data Pointer to data buffer
 * @param length Number of bytes to process
 * @return 32-bit CRC value
 */
uint32_t flash_crc32(const uint8_t *data, uint32_t length);

// ============================================================================
// uint64_t packing helpers (little-endian)
// ============================================================================
/**
 * @brief Build uint64_t from two uint32_t values
 * @param low Lower 32 bits
 * @param high Upper 32 bits
 * @return Packed 64-bit value
 */
uint64_t flash_make_dword(uint32_t low, uint32_t high);

/**
 * @brief Extract lower 32 bits from uint64_t
 * @param dword 64-bit value
 * @return Lower 32 bits
 */
uint32_t flash_get_low_word(uint64_t dword);

/**
 * @brief Extract upper 32 bits from uint64_t
 * @param dword 64-bit value
 * @return Upper 32 bits
 */
uint32_t flash_get_high_word(uint64_t dword);

// ============================================================================
// Float <-> uint32_t conversion (type punning via union)
// ============================================================================
/**
 * @brief Convert float to uint32_t (bit pattern preservation)
 * @param val Float value
 * @return uint32_t representation
 */
uint32_t flash_float_to_u32(float val);

/**
 * @brief Convert uint32_t to float (bit pattern preservation)
 * @param val uint32_t value
 * @return Float representation
 */
float flash_u32_to_float(uint32_t val);

// ============================================================================
// Flash operations
// ============================================================================
/**
 * @brief Erase a single flash page
 * @param page_number Page number to erase (0-127 for STM32G474CC)
 * @return true if erase successful, false otherwise
 */
bool flash_page_erase(uint32_t page_number);

/**
 * @brief Unlock flash for programming
 */
void flash_unlock(void);

/**
 * @brief Lock flash after programming
 */
void flash_lock(void);

/**
 * @brief Clear all flash error flags
 */
void flash_clear_errors(void);

#endif // FLASH_UTILS_H
