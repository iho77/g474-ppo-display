/**
 * @file screen_sensor_data.c
 * @brief Sensor Data screen - Raw sensor readings and diagnostics
 *
 * Displays raw ADC values, voltages, and sensor health information.
 * BTN_M returns to menu.
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "sensor.h"
#include "sensor_diagnostics.h"
#include "flash_storage.h"
#include <stdio.h>

// ============================================================================
// PRIVATE VARIABLES - Widget references for update() callback
// ============================================================================
static lv_obj_t* lbl_s1_raw = NULL;
static lv_obj_t* lbl_s2_raw = NULL;
static lv_obj_t* lbl_s3_raw = NULL;
static lv_obj_t* lbl_vref = NULL;

// Diagnostic labels
static lv_obj_t* lbl_sensor_diags[3] = {NULL, NULL, NULL};
static lv_obj_t* lbl_history = NULL;
static lv_obj_t* lbl_legend = NULL;

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {
    // Title
    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_obj_set_pos(lbl_title, 10, 5);
    lv_label_set_text(lbl_title, "Sensor Data & Diagnostics");
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);

    // Raw sensor values - compact layout
    lbl_s1_raw = lv_label_create(scr);
    lv_obj_set_pos(lbl_s1_raw, 10, 30);
    lv_label_set_text(lbl_s1_raw, "S1:--- S2:--- S3:--- uV");
    lv_obj_set_style_text_color(lbl_s1_raw, lv_color_white(), 0);

    // Note: s2_raw and s3_raw kept for compatibility but not displayed separately
    lbl_s2_raw = NULL;
    lbl_s3_raw = NULL;

    lbl_vref = lv_label_create(scr);
    lv_obj_set_pos(lbl_vref, 10, 50);
    lv_label_set_text(lbl_vref, "VREF: --- mV");
    lv_obj_set_style_text_color(lbl_vref, lv_color_white(), 0);

    // Diagnostics section (moved up)
    lbl_sensor_diags[0] = lv_label_create(scr);
    lv_obj_set_pos(lbl_sensor_diags[0], 10, 75);
    lv_label_set_text(lbl_sensor_diags[0], "S1: ---");
    lv_obj_set_style_text_color(lbl_sensor_diags[0], lv_color_white(), 0);

    lbl_sensor_diags[1] = lv_label_create(scr);
    lv_obj_set_pos(lbl_sensor_diags[1], 10, 95);
    lv_label_set_text(lbl_sensor_diags[1], "S2: ---");
    lv_obj_set_style_text_color(lbl_sensor_diags[1], lv_color_white(), 0);

    lbl_sensor_diags[2] = lv_label_create(scr);
    lv_obj_set_pos(lbl_sensor_diags[2], 10, 115);
    lv_label_set_text(lbl_sensor_diags[2], "S3: ---");
    lv_obj_set_style_text_color(lbl_sensor_diags[2], lv_color_white(), 0);

    lbl_history = lv_label_create(scr);
    lv_obj_set_pos(lbl_history, 10, 140);
    lv_label_set_text(lbl_history, "History: 0");
    lv_obj_set_style_text_color(lbl_history, lv_color_white(), 0);

    // Compact legend (single line, smaller text)
    lbl_legend = lv_label_create(scr);
    lv_obj_set_pos(lbl_legend, 10, 165);
    lv_label_set_text(lbl_legend, "R:0.9-1.0=good A:>1.2=bad SD:<0.01=stable");
    lv_obj_set_style_text_color(lbl_legend, lv_color_make(150, 150, 150), 0);
}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the sensor data screen
 */
lv_obj_t* screen_sensor_data_create(void) {
    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Build screen content
    build_screen_content(scr);

    return scr;
}

/**
 * @brief Periodic update - called at 1Hz while this screen is active
 */
void screen_sensor_data_update(void) {
    if (lbl_s1_raw == NULL) return;

    extern volatile sensor_data_t sensor_data;
    extern uint32_t vref_effective_mv;
    static char buf[80];  // Static to avoid dangling pointer during LVGL refresh

    // Update raw sensor values in microvolts (all on one line)
    (void)snprintf(buf, sizeof(buf), "S1:%d S2:%d S3:%d uV",
                   (int)sensor_data.o2_s_uv[0],
                   (int)sensor_data.o2_s_uv[1],
                   (int)sensor_data.o2_s_uv[2]);
    lv_label_set_text(lbl_s1_raw, buf);

    // Update VREF
    (void)snprintf(buf, sizeof(buf), "VREF: %u mV", (unsigned int)vref_effective_mv);
    lv_label_set_text(lbl_vref, buf);

    // Update diagnostics
    sensor_diag_t results[3];

    // Check calibration count first
    uint32_t raw_count = flash_calibration_get_count();
    if (raw_count < 2) {
        // Not enough calibration history for diagnostics
        (void)snprintf(buf, sizeof(buf), "History: %u cal (need 2+ for diagnostics)",
                       (unsigned int)raw_count);
        lv_label_set_text(lbl_history, buf);
        for (int i = 0; i < 3; i++) {
            lv_label_set_text(lbl_sensor_diags[i], "Need more calibrations");
        }
        return;  // Exit early
    }

    if (sensor_diagnostics_calculate(results)) {
        for (int i = 0; i < 3; i++) {
            if (results[i].valid) {
                // Convert floats to fixed-point integers to avoid float printf
                int32_t r   = (int32_t)(results[i].sensitivity_ratio  * 100.0f + 0.5f);
                int32_t a   = (int32_t)(results[i].degradation_accel  * 100.0f + 0.5f);
                int32_t sd  = (int32_t)(results[i].slope_stddev       * 1000.0f + 0.5f);
                // Check metric availability based on sample count
                if (results[i].sample_count >= 10) {
                    // All metrics available
                    (void)snprintf(buf, sizeof(buf),
                        "S%d: R:%d.%02d A:%d.%02d SD:%d.%03d%s",
                        i + 1,
                        (int)(r / 100), (int)(r % 100),
                        (int)(a / 100), (int)(a % 100),
                        (int)(sd / 1000), (int)(sd % 1000),
                        (results[i].degradation_accel > 1.2f) ? " !" : ""
                    );
                } else if (results[i].sample_count >= 5) {
                    // Ratio + StdDev only
                    (void)snprintf(buf, sizeof(buf),
                        "S%d: R:%d.%02d SD:%d.%03d",
                        i + 1,
                        (int)(r / 100), (int)(r % 100),
                        (int)(sd / 1000), (int)(sd % 1000)
                    );
                } else {
                    // Ratio only
                    (void)snprintf(buf, sizeof(buf),
                        "S%d: R:%d.%02d",
                        i + 1,
                        (int)(r / 100), (int)(r % 100)
                    );
                }
            } else {
                (void)snprintf(buf, sizeof(buf), "S%d: Need more calibrations", i + 1);
            }
            lv_label_set_text(lbl_sensor_diags[i], buf);
        }

        (void)snprintf(buf, sizeof(buf), "History: %d calibrations", results[0].sample_count);
        lv_label_set_text(lbl_history, buf);
    } else {
        // This shouldn't happen since we checked count >= 2 above
        lv_label_set_text(lbl_history, "Error: diagnostics failed");
        for (int i = 0; i < 3; i++) {
            lv_label_set_text(lbl_sensor_diags[i], "---");
        }
    }
}
