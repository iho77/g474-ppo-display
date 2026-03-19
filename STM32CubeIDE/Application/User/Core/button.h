/**
 * @file button.h
 * @brief Button handling API for piezo buttons with interrupt detection
 *
 * Supports two buttons: BTN_M (PA8) and BTN_S (PA9)
 * Active-high configuration: pulled down when idle, HIGH pulse on press
 * Uses EXTI rising-edge interrupt for instant impulse detection
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Lockout time in milliseconds after button press
 * Prevents double-triggers from mechanical switch bounce
 *
 * IMPORTANT: This value is tuned for MECHANICAL SWITCHES during prototyping.
 * For final piezo buttons (no bounce), reduce to 50ms.
 *
 * Mechanical switches: 200-300ms recommended
 * Piezo buttons: 50ms sufficient
 */
#define BTN_LOCKOUT_MS  50   // Piezo buttons: no mechanical bounce

/**
 * @brief Button event types
 */
typedef enum {
    BTN_NONE = 0,      /**< No button event */
    BTN_M_PRESS,       /**< BTN_M press detected */
    BTN_S_PRESS        /**< BTN_S press detected */
} btn_event_t;

/**
 * @brief Initialize button state
 * @note GPIO and NVIC configuration is done in MX_GPIO_Init()
 * Call once during system initialization
 */
void button_init(void);

/**
 * @brief Decrement lockout counters (call every 1ms from SysTick)
 * @note Must be called from SysTick_Handler at 1ms intervals
 */
void button_tick(void);

/**
 * @brief Get pending button event and clear it
 * @return Button event (BTN_NONE if no event pending)
 */
btn_event_t button_get_event(void);

/**
 * @brief Clear any pending button event without returning it
 * @note Use during screen transitions to prevent stale events
 */
void button_clear_event(void);

/**
 * @brief Handle button interrupt (call from HAL_GPIO_EXTI_Callback)
 * @param GPIO_Pin The GPIO pin that triggered the interrupt
 * @note Called from interrupt context - keep fast!
 */
void button_exti_callback(uint16_t GPIO_Pin);

#endif // BUTTON_H
