/**
  ******************************************************************************
  * @file           : ms5837.c
  * @brief          : MS5837-30BA pressure sensor driver implementation
  * @author         : Migrated from MS5849 driver for PPO_Display project
  * @date           : 2026-02-13
  ******************************************************************************
  * @attention
  *
  * Minimal blocking driver for MS5837-30BA 30-bar absolute pressure sensor.
  * Uses HAL I2C blocking API with timeout for initial bring-up and testing.
  *
  * CRITICAL CHANGES from MS5849:
  * - PROM read: 7 coefficients instead of 8
  * - CRC extraction from C[0][15:12]
  * - Second-order compensation: Added HIGH temperature path (>= 20°C)
  * - Updated low-temperature coefficients per MS5837 datasheet
  *
  ******************************************************************************
  */

#include "ms5837.h"
#include <string.h>

/* ==================== Private Helper Functions ==================== */

/**
 * @brief Get conversion delay in milliseconds for given OSR level
 *
 * @param osr Oversampling ratio (MS5837_OSR_xxx)
 * @return uint8_t Delay in milliseconds
 */
static uint8_t ms5837_get_osr_delay(uint8_t osr)
{
    switch (osr) {
        case MS5837_OSR_256:  return MS5837_CONV_DELAY_256;
        case MS5837_OSR_512:  return MS5837_CONV_DELAY_512;
        case MS5837_OSR_1024: return MS5837_CONV_DELAY_1024;
        case MS5837_OSR_2048: return MS5837_CONV_DELAY_2048;
        case MS5837_OSR_4096: return MS5837_CONV_DELAY_4096;
        case MS5837_OSR_8192: return MS5837_CONV_DELAY_8192;
        default:              return MS5837_CONV_DELAY_4096; // Default to OSR 4096
    }
}

/* ==================== Core Driver Functions ==================== */

/**
 * @brief Reset MS5837 sensor and reload PROM to registers
 */
ms5837_status_t ms5837_reset(I2C_HandleTypeDef *hi2c)
{
    uint8_t cmd = MS5837_CMD_RESET;
    HAL_StatusTypeDef hal_status;

    // Send reset command
    hal_status = HAL_I2C_Master_Transmit(hi2c, MS5837_I2C_ADDR << 1, &cmd, 1, MS5837_I2C_TIMEOUT);

    if (hal_status != HAL_OK) {
        return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
    }

    // Wait for PROM reload (datasheet specifies 2.8 ms max)
    HAL_Delay(MS5837_RESET_DELAY);

    return MS5837_OK;
}

/**
 * @brief Read all PROM calibration coefficients (C0-C6) and extract CRC
 */
ms5837_status_t ms5837_read_prom(I2C_HandleTypeDef *hi2c, ms5837_calib_t *calib)
{
    HAL_StatusTypeDef hal_status;
    uint8_t prom_addr;
    uint8_t prom_data[2];

    // Read 7 PROM locations (0xA0, 0xA2, 0xA4, ..., 0xAC)
    for (uint8_t i = 0; i < 7; i++) {
        prom_addr = MS5837_CMD_PROM_READ + (i * 2);

        // Send PROM read command
        hal_status = HAL_I2C_Master_Transmit(hi2c, MS5837_I2C_ADDR << 1, &prom_addr, 1, MS5837_I2C_TIMEOUT);
        if (hal_status != HAL_OK) {
            return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
        }

        // Read 16-bit coefficient (MSB first)
        hal_status = HAL_I2C_Master_Receive(hi2c, MS5837_I2C_ADDR << 1, prom_data, 2, MS5837_I2C_TIMEOUT);
        if (hal_status != HAL_OK) {
            return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
        }

        // Combine bytes (big-endian)
        calib->C[i] = ((uint16_t)prom_data[0] << 8) | prom_data[1];
    }

    // Extract CRC from C[0][15:12]
    calib->crc_prom = (calib->C[0] >> 12) & 0x0F;

    return MS5837_OK;
}

/**
 * @brief Read 24-bit ADC conversion result
 */
ms5837_status_t ms5837_read_adc(I2C_HandleTypeDef *hi2c, uint32_t *adc_value)
{
    HAL_StatusTypeDef hal_status;
    uint8_t cmd = MS5837_CMD_ADC_READ;
    uint8_t adc_data[3];

    // Send ADC read command
    hal_status = HAL_I2C_Master_Transmit(hi2c, MS5837_I2C_ADDR << 1, &cmd, 1, MS5837_I2C_TIMEOUT);
    if (hal_status != HAL_OK) {
        return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
    }

    // Read 24-bit ADC value (MSB first)
    hal_status = HAL_I2C_Master_Receive(hi2c, MS5837_I2C_ADDR << 1, adc_data, 3, MS5837_I2C_TIMEOUT);
    if (hal_status != HAL_OK) {
        return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
    }

    // Combine 3 bytes to 24-bit value (big-endian)
    *adc_value = ((uint32_t)adc_data[0] << 16) | ((uint32_t)adc_data[1] << 8) | adc_data[2];

    return MS5837_OK;
}

/**
 * @brief Start D1 (pressure) ADC conversion
 */
ms5837_status_t ms5837_convert_d1(I2C_HandleTypeDef *hi2c, uint8_t osr)
{
    uint8_t cmd = MS5837_CMD_CONV_D1 + osr;
    HAL_StatusTypeDef hal_status;

    // Send conversion command
    hal_status = HAL_I2C_Master_Transmit(hi2c, MS5837_I2C_ADDR << 1, &cmd, 1, MS5837_I2C_TIMEOUT);

    if (hal_status != HAL_OK) {
        return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
    }

    // Wait for conversion to complete
    HAL_Delay(ms5837_get_osr_delay(osr));

    return MS5837_OK;
}

/**
 * @brief Start D2 (temperature) ADC conversion
 */
ms5837_status_t ms5837_convert_d2(I2C_HandleTypeDef *hi2c, uint8_t osr)
{
    uint8_t cmd = MS5837_CMD_CONV_D2 + osr;
    HAL_StatusTypeDef hal_status;

    // Send conversion command
    hal_status = HAL_I2C_Master_Transmit(hi2c, MS5837_I2C_ADDR << 1, &cmd, 1, MS5837_I2C_TIMEOUT);

    if (hal_status != HAL_OK) {
        return (hal_status == HAL_TIMEOUT) ? MS5837_ERROR_TIMEOUT : MS5837_ERROR_I2C;
    }

    // Wait for conversion to complete
    HAL_Delay(ms5837_get_osr_delay(osr));

    return MS5837_OK;
}

/**
 * @brief Calculate pressure and temperature from raw ADC values
 *
 * Implements 2nd order polynomial compensation algorithm from MS5837 datasheet.
 * CRITICAL: MS5837 has DIFFERENT second-order compensation than MS5849:
 * - Low temperature (< 20°C): Updated coefficients
 * - High temperature (>= 20°C): NEW compensation path (not in MS5849)
 */
ms5837_status_t ms5837_calculate(const ms5837_calib_t *calib, uint32_t D1, uint32_t D2, ms5837_data_t *data)
{
    int64_t dT;         // Difference between actual and reference temperature
    int64_t TEMP;       // Actual temperature
    int64_t OFF;        // Offset at actual temperature
    int64_t SENS;       // Sensitivity at actual temperature
    int64_t P;          // Compensated pressure

    // Store raw values
    data->D1_raw = D1;
    data->D2_raw = D2;

    // Calculate temperature
    // dT = D2 - T_REF = D2 - C[5] * 2^8
    dT = (int64_t)D2 - ((int64_t)calib->C[5] << 8);

    // TEMP = 2000 + dT * TEMPSENS = 2000 + dT * C[6] / 2^23
    TEMP = 2000 + ((dT * (int64_t)calib->C[6]) >> 23);

    // Calculate temperature compensated pressure offset
    // OFF = OFF_T1 * 2^17 + TCO * dT / 2^6
    // OFF = C[2] * 2^17 + (C[4] * dT) / 2^6
    OFF = ((int64_t)calib->C[2] << 17) + (((int64_t)calib->C[4] * dT) >> 6);

    // Calculate temperature compensated pressure sensitivity
    // SENS = SENS_T1 * 2^16 + TCS * dT / 2^7
    // SENS = C[1] * 2^16 + (C[3] * dT) / 2^7
    SENS = ((int64_t)calib->C[1] << 16) + (((int64_t)calib->C[3] * dT) >> 7);

    // Calculate compensated pressure
    // P = (D1 * SENS / 2^21 - OFF) / 2^15
    P = ((((int64_t)D1 * SENS) >> 21) - OFF) >> 15;

    // ========== MS5837 SECOND-ORDER TEMPERATURE COMPENSATION ==========
    // CRITICAL: MS5837 has BOTH low AND high temperature paths
    int64_t T2 = 0, OFF2 = 0, SENS2 = 0;

    if (TEMP < 2000) {
        // === LOW TEMPERATURE COMPENSATION (< 20°C) ===
        int64_t TEMP_SQ = (TEMP - 2000) * (TEMP - 2000);

        // Ti = 3 * dT^2 / 2^33
        T2 = (3 * (dT * dT)) >> 33;

        // OFFi = 3 * (TEMP - 2000)^2 / 2 = 1.5 * (TEMP - 2000)^2
        OFF2 = (3 * TEMP_SQ) >> 1;

        // SENSi = 5 * (TEMP - 2000)^2 / 8 = 0.625 * (TEMP - 2000)^2
        SENS2 = (5 * TEMP_SQ) >> 3;

        // Very low temperature correction (< -15°C)
        if (TEMP < -1500) {
            int64_t TEMP_VLOW_SQ = (TEMP + 1500) * (TEMP + 1500);
            OFF2 += 7 * TEMP_VLOW_SQ;
            SENS2 += 4 * TEMP_VLOW_SQ;
        }
    } else {
        // === HIGH TEMPERATURE COMPENSATION (>= 20°C) ===
        // CRITICAL: This path does NOT exist in MS5849
        int64_t TEMP_SQ = (TEMP - 2000) * (TEMP - 2000);

        // Ti = 2 * dT^2 / 2^37
        T2 = (2 * (dT * dT)) >> 37;

        // OFFi = (TEMP - 2000)^2 / 16
        OFF2 = TEMP_SQ >> 4;

        // SENSi = 0
        SENS2 = 0;
    }

    // Apply corrections
    TEMP -= T2;
    OFF -= OFF2;
    SENS -= SENS2;

    // Recalculate pressure with corrected values
    P = ((((int64_t)D1 * SENS) >> 21) - OFF) >> 15;

    // Store results
    data->temperature_c100 = (int32_t)TEMP;  // Temperature in 0.01°C
    data->pressure_mbar = (int32_t)P;        // Pressure in mbar

    return MS5837_OK;
}

/* ==================== High-Level Helper Functions ==================== */

/**
 * @brief Initialise MS5837: reset and read PROM calibration data
 *
 * Intended to be called once at boot with blocking I2C.
 * On success, calib is populated. The DMA FSM handles all subsequent measurements.
 *
 * @param hi2c  Pointer to I2C handle
 * @param calib Pointer to calibration structure (output)
 * @param data  Unused — kept for API compatibility
 * @return MS5837_OK on success, error code otherwise
 */
ms5837_status_t ms5837_init(I2C_HandleTypeDef *hi2c, ms5837_calib_t *calib, ms5837_data_t *data)
{
    ms5837_status_t status;

    status = ms5837_reset(hi2c);
    if (status != MS5837_OK) return status;

    status = ms5837_read_prom(hi2c, calib);
    if (status != MS5837_OK) return status;

    /* Sanity-check PROM: C[1] (pressure sensitivity) must be non-zero */
    if (calib->C[1] == 0) return MS5837_ERROR_I2C;

    return MS5837_OK;  // DMA FSM handles measurements
}

/**
 * @brief Read pressure and temperature in single blocking call
 */
ms5837_status_t ms5837_read_pt(I2C_HandleTypeDef *hi2c, const ms5837_calib_t *calib, uint8_t osr, ms5837_data_t *data)
{
    ms5837_status_t status;
    uint32_t D1, D2;

    // Step 1: Convert D2 (temperature)
    status = ms5837_convert_d2(hi2c, osr);
    if (status != MS5837_OK) {
        return status;
    }

    // Step 2: Read D2 ADC
    status = ms5837_read_adc(hi2c, &D2);
    if (status != MS5837_OK) {
        return status;
    }

    // Step 3: Convert D1 (pressure)
    status = ms5837_convert_d1(hi2c, osr);
    if (status != MS5837_OK) {
        return status;
    }

    // Step 4: Read D1 ADC
    status = ms5837_read_adc(hi2c, &D1);
    if (status != MS5837_OK) {
        return status;
    }

    // Step 5: Guard against ADC-not-ready sentinel (both zero means conversion wasn't complete)
    if (D1 == 0 || D2 == 0) {
        return MS5837_ERROR_I2C;
    }

    // Step 6: Calculate compensated values
    status = ms5837_calculate(calib, D1, D2, data);

    return status;
}
