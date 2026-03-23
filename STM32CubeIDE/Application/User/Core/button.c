/**
 * @file button.c
 * @brief Button handling implementation for piezo buttons
 *
 * Buttons:
 * - BTN_M: PA2 (Mode/Menu button)
 * - BTN_S: PA3 (Select button)
 *
 * Active-high piezo buttons with EXTI rising-edge interrupt detection.
 * Lockout timer prevents double-triggers (50ms after each press).
 */

#include "button.h"
#include "stm32g4xx_hal.h"

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

/** @brief BTN_M GPIO port */
#define BTN_M_GPIO_Port    GPIOA

/** @brief BTN_M GPIO pin */
#define BTN_M_Pin          GPIO_PIN_2

/** @brief BTN_S GPIO port */
#define BTN_S_GPIO_Port    GPIOA

/** @brief BTN_S GPIO pin */
#define BTN_S_Pin          GPIO_PIN_3

// ============================================================================
// PRIVATE STATE
// ============================================================================

/** @brief Pending button event (written by ISR, read by main loop) */
static volatile btn_event_t pending_event = BTN_NONE;

/** @brief Lockout counter for BTN_M in milliseconds */
static volatile uint8_t lockout_m = 0;

/** @brief Lockout counter for BTN_S in milliseconds */
static volatile uint8_t lockout_s = 0;

volatile uint32_t g_dbg_btn_m_set = 0;

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void button_init(void) {
    // Reset state
    pending_event = BTN_NONE;
    lockout_m = 0;
    lockout_s = 0;

    // NOTE: GPIO configuration (EXTI mode, pull-down) is done in MX_GPIO_Init()
    // NOTE: NVIC enable for EXTI9_5_IRQn is done in MX_GPIO_Init()
}

void button_tick(void) {
    // Decrement lockout counters (called every 1ms from SysTick)
    if (lockout_m > 0) {
        lockout_m--;
    }
    if (lockout_s > 0) {
        lockout_s--;
    }
}

btn_event_t button_get_event(void) {
    __disable_irq();
    btn_event_t evt = pending_event;
    pending_event = BTN_NONE;
    __enable_irq();
    return evt;
}

void button_clear_event(void) {
    // Clear pending event without returning it (for screen transitions)
    pending_event = BTN_NONE;
}

void button_exti_callback(uint16_t GPIO_Pin) {
    // Called from HAL_GPIO_EXTI_Callback() in interrupt context

    if (GPIO_Pin == BTN_M_Pin) {
        // BTN_M rising edge detected — confirm pin is still HIGH (rejects glitches)
        if (HAL_GPIO_ReadPin(BTN_M_GPIO_Port, BTN_M_Pin) == GPIO_PIN_RESET)
            return;
        if (lockout_m == 0) {
            pending_event = BTN_M_PRESS;
            lockout_m = BTN_LOCKOUT_MS;
            g_dbg_btn_m_set++;              /* Stage B: event written */
        }
    }
    else if (GPIO_Pin == BTN_S_Pin) {
        // BTN_S rising edge detected — confirm pin is still HIGH (rejects glitches)
        if (HAL_GPIO_ReadPin(BTN_S_GPIO_Port, BTN_S_Pin) == GPIO_PIN_RESET)
            return;
        if (lockout_s == 0) {
            // Only queue BTN_S if BTN_M not already pending (priority)
            if (pending_event != BTN_M_PRESS) {
                pending_event = BTN_S_PRESS;
            }
            lockout_s = BTN_LOCKOUT_MS;
        }
    }
}
