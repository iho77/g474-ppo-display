/**
 * @file warning.c
 * @brief PPO2 warning subsystem implementation
 */

#include "warning.h"
#include "sensor.h"
#include "main.h"
#include <stdlib.h>  // For abs()

/* ============================================================================
 * Vibro motor hardware helpers
 * TIM5 CH1 on PA0 (VIB_PWM): 96 MHz / (PSC+1=960) = 100 kHz tick; ARR=499 → 200 Hz
 * ============================================================================ */

#define VIBRO_PSC   959u
#define VIBRO_ARR   499u

static void vibro_hw_start(uint16_t ccr)
{
    __HAL_TIM_SET_PRESCALER(&htim5, VIBRO_PSC);
    __HAL_TIM_SET_AUTORELOAD(&htim5, VIBRO_ARR);
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, ccr);
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
}

static void vibro_hw_stop(void)
{
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
}

/* ============================================================================
 * Haptic pattern sequences
 * Each step: {duration_ms, motor_on}. Total ≈1 s → re-triggered each 1 Hz update.
 * Duty cycles: DRIFT=50% (CCR=249), NEAR=75% (CCR=374), CRITICAL=~99% (CCR=494)
 * ============================================================================ */

typedef struct {
    uint16_t duration_ms;
    bool     motor_on;
} vibro_step_t;

#define VIBRO_DUTY_DRIFT     249u   /* 50%  */
#define VIBRO_DUTY_NEAR      374u   /* 75%  */
#define VIBRO_DUTY_CRITICAL  494u   /* ~99% */

static const vibro_step_t SEQ_DRIFT[] = {
    {200, true}, {800, false}
};

static const vibro_step_t SEQ_NEAR[] = {
    {150, true}, {150, false}, {150, true}, {550, false}
};

static const vibro_step_t SEQ_CRITICAL[] = {
    {100, true}, {100, false}, {100, true}, {100, false}, {100, true}, {500, false}
};

#define SEQ_LEN(arr)  (sizeof(arr) / sizeof((arr)[0]))

static const vibro_step_t* get_seq(warning_level_t level, uint8_t* out_len)
{
    switch (level) {
        case WARNING_YELLOW_DRIFT:
            *out_len = (uint8_t)SEQ_LEN(SEQ_DRIFT);
            return SEQ_DRIFT;
        case WARNING_NEAR_CRITICAL:
            *out_len = (uint8_t)SEQ_LEN(SEQ_NEAR);
            return SEQ_NEAR;
        case WARNING_CRITICAL:
            *out_len = (uint8_t)SEQ_LEN(SEQ_CRITICAL);
            return SEQ_CRITICAL;
        default:
            *out_len = 0;
            return NULL;
    }
}

static uint16_t get_duty(warning_level_t level)
{
    switch (level) {
        case WARNING_YELLOW_DRIFT:   return VIBRO_DUTY_DRIFT;
        case WARNING_NEAR_CRITICAL:  return VIBRO_DUTY_NEAR;
        case WARNING_CRITICAL:       return VIBRO_DUTY_CRITICAL;
        default:                     return 0;
    }
}

/* ============================================================================
 * Vibro state machine
 * ============================================================================ */

static warning_level_t g_vibro_level         = WARNING_NONE;
static warning_level_t g_vibro_snoozed_level = WARNING_NONE;
static uint8_t         g_vibro_step          = 0;
static uint16_t        g_vibro_remaining_ms  = 0;

static void vibro_start_pattern(warning_level_t level)
{
    uint8_t len;
    const vibro_step_t* seq = get_seq(level, &len);
    if (seq == NULL || len == 0) return;

    g_vibro_level = level;
    g_vibro_step  = 0;
    g_vibro_remaining_ms = seq[0].duration_ms;

    if (seq[0].motor_on) {
        vibro_hw_start(get_duty(level));
    } else {
        vibro_hw_stop();
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

bool vibro_is_active(void)
{
    if (g_vibro_level == WARNING_NONE) return false;
    uint8_t len;
    get_seq(g_vibro_level, &len);
    return g_vibro_step < len;
}

void vibro_acknowledge(void)
{
    g_vibro_snoozed_level = g_vibro_level;
    g_vibro_level         = WARNING_NONE;
    g_vibro_step          = 0;
    g_vibro_remaining_ms  = 0;
    vibro_hw_stop();
}

void vibro_tick_5ms(void)
{
    if (g_vibro_level == WARNING_NONE) return;

    uint8_t len;
    const vibro_step_t* seq = get_seq(g_vibro_level, &len);
    if (seq == NULL || g_vibro_step >= len) return;  /* pattern done, idle */

    if (g_vibro_remaining_ms > 5u) {
        g_vibro_remaining_ms -= 5u;
        return;
    }

    /* Current step expired — advance */
    g_vibro_step++;
    if (g_vibro_step >= len) {
        /* Pattern complete — stop and idle until next 1Hz re-trigger */
        vibro_hw_stop();
        return;
    }

    /* Apply next step */
    g_vibro_remaining_ms = seq[g_vibro_step].duration_ms;
    if (seq[g_vibro_step].motor_on) {
        vibro_hw_start(get_duty(g_vibro_level));
    } else {
        vibro_hw_stop();
    }
}

void warning_trigger_vibration(warning_level_t level)
{
    if (level == WARNING_NONE) {
        /* Clear everything including snooze */
        vibro_hw_stop();
        g_vibro_level         = WARNING_NONE;
        g_vibro_snoozed_level = WARNING_NONE;
        g_vibro_step          = 0;
        g_vibro_remaining_ms  = 0;
        return;
    }

    /* Snoozed at this exact level — do nothing */
    if (level == g_vibro_snoozed_level) return;

    /* Level changed → clear snooze, start new pattern */
    if (level != g_vibro_level) {
        g_vibro_snoozed_level = WARNING_NONE;
        vibro_start_pattern(level);
        return;
    }

    /* Same level: re-trigger only if pattern has finished */
    uint8_t len;
    get_seq(g_vibro_level, &len);
    if (g_vibro_step >= len) {
        vibro_start_pattern(level);
    }
}

/* ============================================================================
 * Warning level detection
 * ============================================================================ */

warning_level_t warning_check_sensor(uint8_t sensor_index) {
    // 1. Check sensor validity
    if (sensor_index >= (SENSOR_COUNT - 2) || !sensor_data.s_valid[sensor_index]) {
        return WARNING_NONE;
    }

    // 2. Get sensor PPO reading (mbar)
    int32_t ppo = sensor_data.o2_s_ppo[sensor_index];

    // 3. Get thresholds from settings
    uint16_t min_ppo = sensor_data.settings.min_ppo_threshold;
    uint16_t max_ppo = sensor_data.settings.max_ppo_threshold;
    uint16_t warning_dist = sensor_data.settings.warning_ppo_threshold;

    // 4. Get active setpoint value
    uint16_t setpoint_value;
    if (active_setpoint == 1) {
        setpoint_value = sensor_data.settings.ppo_setpoint1;
    } else {
        setpoint_value = sensor_data.settings.ppo_setpoint2;
    }

    // 5. Check warning levels (PRIORITY ORDER - highest first)

    // CRITICAL: Exceeded min/max limits
    if (ppo < min_ppo || ppo > max_ppo) {
        return WARNING_CRITICAL;
    }

    // NEAR_CRITICAL: Within warning distance of limits
    // Cast to int32_t to prevent unsigned overflow/underflow
    if (ppo <= ((int32_t)min_ppo + (int32_t)warning_dist) ||
        ppo >= ((int32_t)max_ppo - (int32_t)warning_dist)) {
        return WARNING_NEAR_CRITICAL;
    }

    // YELLOW_DRIFT: Drifted from active setpoint
    int32_t drift = ppo - setpoint_value;
    if (drift < 0) drift = -drift;  // Absolute value
    if (drift > warning_dist) {
        return WARNING_YELLOW_DRIFT;
    }

    // NONE: All good
    return WARNING_NONE;
}

/* ============================================================================
 * Visual styling
 * ============================================================================ */

void warning_apply_style(uint8_t sensor_index, warning_level_t level,
                         lv_obj_t* panel_obj, lv_obj_t* label_obj) {
    uint32_t border_color, text_color, panel_bg_color, label_bg_color;
    lv_opa_t panel_bg_opa, label_bg_opa;

    // Suppress unused parameter warning
    (void)sensor_index;

    switch (level) {
        case WARNING_CRITICAL:
            border_color = 0xFF0000;      // Red border
            text_color = 0xFFFFFF;        // White text
            panel_bg_color = 0xFF0000;    // Red panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background (transparent)
            label_bg_opa = LV_OPA_TRANSP; // Transparent to show panel background
            break;

        case WARNING_NEAR_CRITICAL:
            border_color = 0xFF0000;      // Red border
            text_color = 0xFF0000;        // Red text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;

        case WARNING_YELLOW_DRIFT:
            border_color = 0xFFFF00;      // Yellow border
            text_color = 0xFFFF00;        // Yellow text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;

        case WARNING_NONE:
        default:
            border_color = 0x00FF00;      // Green border
            text_color = 0xFFFFFF;        // White text
            panel_bg_color = 0x000000;    // Black panel background
            panel_bg_opa = LV_OPA_COVER;
            label_bg_color = 0x000000;    // Black label background
            label_bg_opa = LV_OPA_TRANSP;
            break;
    }

    // Apply border color and background to panel
    if (panel_obj) {
        lv_obj_set_style_border_color(panel_obj, lv_color_hex(border_color),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(panel_obj, lv_color_hex(panel_bg_color),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(panel_obj, panel_bg_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // Apply text color and background to label
    if (label_obj) {
        lv_obj_set_style_text_color(label_obj, lv_color_hex(text_color),
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(label_obj, lv_color_hex(label_bg_color),
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(label_obj, label_bg_opa,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}
