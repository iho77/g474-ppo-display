/**
 * @file mock_stm32.h
 * @brief HAL/LL stubs for native x86 unit testing
 *
 * Replaces stm32g4xx_hal.h and stm32g4xx_ll_adc.h with lightweight stubs
 * so production .c files compile on a native GCC x86 host.
 *
 * Rules:
 *  - Macros expand to no-ops or safe zero values
 *  - Types match the ARM HAL ABI widths (uint32_t etc.)
 *  - Structs carry only the fields actually touched by the modules under test
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Basic typedefs that HAL normally provides ─────────────────────────────── */

typedef enum {
    HAL_OK      = 0x00U,
    HAL_ERROR   = 0x01U,
    HAL_BUSY    = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

/* Minimal TIM handle — only fields referenced by warning.c */
typedef struct {
    uint32_t dummy;
} TIM_HandleTypeDef;

/* Minimal I2C handle — only fields referenced by ms5837.c */
typedef struct {
    uint32_t dummy;
} I2C_HandleTypeDef;

/* Minimal SPI handle — referenced by ext_flash_storage.h (not compiled in tests) */
typedef struct {
    uint32_t dummy;
} SPI_HandleTypeDef;

/* ── IRQ stub ───────────────────────────────────────────────────────────────── */
#define __disable_irq()  ((void)0)
#define __enable_irq()   ((void)0)

/* ── TIM macros (used by warning.c vibro helpers) ───────────────────────────── */
#define __HAL_TIM_SET_PRESCALER(htim, psc)    ((void)(psc))
#define __HAL_TIM_SET_AUTORELOAD(htim, arr)   ((void)(arr))
#define __HAL_TIM_SET_COMPARE(htim, ch, ccr)  ((void)(ccr))
#define TIM_CHANNEL_1  1U

static inline HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t ch) {
    (void)htim; (void)ch; return HAL_OK;
}
static inline HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t ch) {
    (void)htim; (void)ch; return HAL_OK;
}

/* ── FLASH stubs (flash_utils.c HAL-dependent functions not under test) ──────── */
static inline HAL_StatusTypeDef HAL_FLASH_Unlock(void) { return HAL_OK; }
static inline HAL_StatusTypeDef HAL_FLASH_Lock(void)   { return HAL_OK; }

typedef struct {
    uint32_t TypeErase;
    uint32_t Banks;
    uint32_t Page;
    uint32_t NbPages;
} FLASH_EraseInitTypeDef;

#define FLASH_TYPEERASE_PAGES  0x01U
#define FLASH_BANK_1           0x01U
#define FLASH_FLAG_ALL_ERRORS  0xFFFFFFFFU

#define __HAL_FLASH_CLEAR_FLAG(flags)  ((void)(flags))

static inline HAL_StatusTypeDef HAL_FLASHEx_Erase(
        FLASH_EraseInitTypeDef *pEraseInit, uint32_t *PageError) {
    (void)pEraseInit;
    if (PageError) *PageError = 0xFFFFFFFFU;
    return HAL_OK;
}
static inline HAL_StatusTypeDef HAL_FLASH_Program(
        uint32_t type, uint32_t addr, uint64_t data) {
    (void)type; (void)addr; (void)data; return HAL_OK;
}
static inline uint32_t HAL_FLASH_GetError(void) { return 0; }

/* ── I2C stubs (ms5837.c — only ms5837_calculate() tested) ─────────────────── */
static inline HAL_StatusTypeDef HAL_I2C_Master_Transmit(
        I2C_HandleTypeDef *hi2c, uint16_t addr,
        uint8_t *buf, uint16_t size, uint32_t timeout) {
    (void)hi2c; (void)addr; (void)buf; (void)size; (void)timeout;
    return HAL_OK;
}
static inline HAL_StatusTypeDef HAL_I2C_Master_Receive(
        I2C_HandleTypeDef *hi2c, uint16_t addr,
        uint8_t *buf, uint16_t size, uint32_t timeout) {
    (void)hi2c; (void)addr; (void)buf; (void)size; (void)timeout;
    return HAL_OK;
}
static inline HAL_StatusTypeDef HAL_I2C_Master_Receive_IT(
        I2C_HandleTypeDef *hi2c, uint16_t addr,
        uint8_t *buf, uint16_t size) {
    (void)hi2c; (void)addr; (void)buf; (void)size;
    return HAL_OK;
}
static inline void HAL_Delay(uint32_t ms) { (void)ms; }

/* ── LL ADC macro stubs (sensor.c uses __LL_ADC_CALC_VREFANALOG_VOLTAGE) ────── */
#define LL_ADC_RESOLUTION_12B  0U
#define __LL_ADC_CALC_VREFANALOG_VOLTAGE(vrefint_data, resolution) \
    ((uint32_t)(3300U))   /* Return fixed 3300mV for unit tests */
#define __LL_ADC_CALC_DATA_TO_VOLTAGE(vref_mv, data, resolution) \
    ((uint32_t)((data) * (vref_mv) / 4095U))

/* ── GPIO stubs ─────────────────────────────────────────────────────────────── */
typedef struct { uint32_t dummy; } GPIO_TypeDef;
#define GPIO_PIN_SET   1U
#define GPIO_PIN_RESET 0U
static inline void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, uint32_t s) {
    (void)p; (void)pin; (void)s;
}

/* ── Suppress "stm32g4xx_hal.h" include guard so modules compile ─────────────── */
#define STM32G4XX_HAL_H
#define __STM32G4xx_HAL_H

/* ── HAL handle externs expected by warning.c (defined in mock_sensor_data.c) ── */
extern TIM_HandleTypeDef htim5;
