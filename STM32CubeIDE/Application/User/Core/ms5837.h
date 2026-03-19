/**
  ******************************************************************************
  * @file           : ms5837.h
  * @brief          : MS5837-30BA pressure sensor driver header
  * @author         : Migrated from MS5849 driver for PPO_Display project
  * @date           : 2026-02-13
  ******************************************************************************
  * @attention
  *
  * Minimal blocking driver for MS5837-30BA 30-bar absolute pressure sensor.
  * Compatible with MS5803 command set family.
  * I2C address: 0x76 (fixed, no CSB option)
  *
  * CRITICAL DIFFERENCES from MS5849:
  * - PROM has 7 coefficients (C[0-6]) instead of 8
  * - CRC is 4-bit field in C[0][15:12], not separate C[7]
  * - Second-order compensation includes HIGH temperature path (>= 20°C)
  * - Faster conversion timing (9.04ms vs 9.5ms @ OSR 4096)
  *
  ******************************************************************************
  */

#ifndef MS5837_H
#define MS5837_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* I2C Configuration */
// MS5837 has FIXED I2C address (no CSB option like MS5849)
#define MS5837_I2C_ADDR         0x76    // 7-bit address (HAL auto-shifts)
#define MS5837_I2C_TIMEOUT      100     // I2C timeout in milliseconds

/* Command Set */
#define MS5837_CMD_RESET        0x1E    // Reset and reload PROM
#define MS5837_CMD_CONV_D1      0x40    // Convert D1 (pressure), add OSR
#define MS5837_CMD_CONV_D2      0x50    // Convert D2 (temperature), add OSR
#define MS5837_CMD_ADC_READ     0x00    // Read ADC result (24-bit)
#define MS5837_CMD_PROM_READ    0xA0    // PROM read base address

/* OSR (Oversampling Ratio) Levels - Add to conversion command */
#define MS5837_OSR_256          0x00    // Conversion time: 0.6 ms
#define MS5837_OSR_512          0x02    // Conversion time: 1.2 ms
#define MS5837_OSR_1024         0x04    // Conversion time: 2.3 ms
#define MS5837_OSR_2048         0x06    // Conversion time: 4.5 ms
#define MS5837_OSR_4096         0x08    // Conversion time: 9.04 ms (recommended)
#define MS5837_OSR_8192         0x0A    // Conversion time: 18 ms

/* Timing Constants (ms) */
#define MS5837_RESET_DELAY      3       // Delay after reset command
#define MS5837_CONV_DELAY_256   1       // OSR 256 conversion time
#define MS5837_CONV_DELAY_512   2       // OSR 512 conversion time
#define MS5837_CONV_DELAY_1024  3       // OSR 1024 conversion time
#define MS5837_CONV_DELAY_2048  5       // OSR 2048 conversion time
#define MS5837_CONV_DELAY_4096  12      // OSR 4096 conversion time (9.04ms max per datasheet + 3ms HAL_Delay margin)
#define MS5837_CONV_DELAY_8192  18      // OSR 8192 conversion time

/**
 * @brief Driver status codes
 */
typedef enum {
    MS5837_OK = 0,              // Operation successful
    MS5837_ERROR_I2C,           // I2C communication error
    MS5837_ERROR_TIMEOUT,       // I2C timeout
    MS5837_ERROR_CRC            // CRC validation failed (reserved)
} ms5837_status_t;

/**
 * @brief PROM calibration coefficients
 *
 * Factory-calibrated coefficients stored in sensor PROM.
 * MS5837 PROM layout (7 locations, 0xA0-0xAC):
 * C[0][15:12] = 4-bit CRC
 * C[0][11:0]  = Manufacturer reserved
 * C[1] = Pressure sensitivity (SENS_T1)
 * C[2] = Pressure offset (OFF_T1)
 * C[3] = Temperature coefficient of pressure sensitivity (TCS)
 * C[4] = Temperature coefficient of pressure offset (TCO)
 * C[5] = Reference temperature (T_REF)
 * C[6] = Temperature coefficient of temperature (TEMPSENS)
 */
typedef struct {
    uint16_t C[7];              // Calibration coefficients C[0-6]
    uint8_t crc_prom;           // Extracted 4-bit CRC from C[0][15:12]
} ms5837_calib_t;

/**
 * @brief Measurement data structure
 *
 * Contains both raw ADC values and calculated pressure/temperature.
 */
typedef struct {
    int32_t pressure_mbar;      // Pressure in millibar (1 mbar ≈ 1 cm depth)
    int32_t temperature_c100;   // Temperature in centidegrees (2500 = 25.00°C)
    uint32_t D1_raw;           // Raw pressure ADC value (24-bit)
    uint32_t D2_raw;           // Raw temperature ADC value (24-bit)
} ms5837_data_t;

/* ==================== Core Driver Functions ==================== */

/**
 * @brief Reset MS5837 sensor and reload PROM to registers
 *
 * @param hi2c Pointer to I2C handle
 * @return ms5837_status_t Operation status
 */
ms5837_status_t ms5837_reset(I2C_HandleTypeDef *hi2c);

/**
 * @brief Read all PROM calibration coefficients (C0-C6) and extract CRC
 *
 * @param hi2c Pointer to I2C handle
 * @param calib Pointer to calibration structure to store coefficients
 * @return ms5837_status_t Operation status
 */
ms5837_status_t ms5837_read_prom(I2C_HandleTypeDef *hi2c, ms5837_calib_t *calib);

/**
 * @brief Read 24-bit ADC conversion result
 *
 * @param hi2c Pointer to I2C handle
 * @param adc_value Pointer to store 24-bit ADC value
 * @return ms5837_status_t Operation status
 */
ms5837_status_t ms5837_read_adc(I2C_HandleTypeDef *hi2c, uint32_t *adc_value);

/**
 * @brief Start D1 (pressure) ADC conversion
 *
 * @param hi2c Pointer to I2C handle
 * @param osr Oversampling ratio (MS5837_OSR_xxx)
 * @return ms5837_status_t Operation status
 *
 * @note Call ms5837_read_adc() after conversion delay
 */
ms5837_status_t ms5837_convert_d1(I2C_HandleTypeDef *hi2c, uint8_t osr);

/**
 * @brief Start D2 (temperature) ADC conversion
 *
 * @param hi2c Pointer to I2C handle
 * @param osr Oversampling ratio (MS5837_OSR_xxx)
 * @return ms5837_status_t Operation status
 *
 * @note Call ms5837_read_adc() after conversion delay
 */
ms5837_status_t ms5837_convert_d2(I2C_HandleTypeDef *hi2c, uint8_t osr);

/**
 * @brief Calculate pressure and temperature from raw ADC values
 *
 * Implements 2nd order polynomial compensation per MS5837 datasheet.
 * CRITICAL: MS5837 uses DIFFERENT compensation formulas than MS5849:
 * - Low temperature (< 20°C): Updated coefficients
 * - High temperature (>= 20°C): New compensation path (not present in MS5849)
 *
 * @param calib Pointer to calibration coefficients
 * @param D1 Raw pressure ADC value
 * @param D2 Raw temperature ADC value
 * @param data Pointer to data structure to store results
 * @return ms5837_status_t Operation status
 */
ms5837_status_t ms5837_calculate(const ms5837_calib_t *calib, uint32_t D1, uint32_t D2, ms5837_data_t *data);

/* ==================== High-Level Helper Functions ==================== */

/**
 * @brief Initialise MS5837: reset, read PROM, take first measurement
 *
 * Call once at boot with blocking I2C before starting the DMA FSM.
 *
 * @param hi2c  Pointer to I2C handle
 * @param calib Pointer to calibration structure (output)
 * @param data  Pointer to measurement structure (output, first reading)
 * @return MS5837_OK on success, error code otherwise
 */
ms5837_status_t ms5837_init(I2C_HandleTypeDef *hi2c, ms5837_calib_t *calib, ms5837_data_t *data);

/**
 * @brief Read pressure and temperature in single blocking call
 *
 * Performs full measurement sequence:
 * 1. Convert D2 (temperature)
 * 2. Read D2 ADC
 * 3. Convert D1 (pressure)
 * 4. Read D1 ADC
 * 5. Calculate compensated values
 *
 * @param hi2c Pointer to I2C handle
 * @param calib Pointer to calibration coefficients
 * @param osr Oversampling ratio (MS5837_OSR_xxx)
 * @param data Pointer to data structure to store results
 * @return ms5837_status_t Operation status
 *
 * @note Blocking call, total time ~20 ms @ OSR 4096
 */
ms5837_status_t ms5837_read_pt(I2C_HandleTypeDef *hi2c, const ms5837_calib_t *calib, uint8_t osr, ms5837_data_t *data);

#ifdef __cplusplus
}
#endif

#endif /* MS5837_H */
