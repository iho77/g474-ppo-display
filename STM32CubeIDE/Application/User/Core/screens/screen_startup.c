/**
 * @file screen_startup.c
 * @brief Startup screen - Boot splash and initialization status
 *
 * Displays during system boot and auto-advances to Main screen after calibration.
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "sensor.h"
#include "flash_storage.h"
#include <stdio.h>

// ============================================================================
// PRIVATE VARIABLES - Widget references for update() callback
// ============================================================================
static lv_obj_t* lbl_status = NULL;
static uint8_t startup_ticks = 0;
static char status_buf1[] = "|";  // Static buffer to avoid stack issues
static char status_buf2[] = "--";
lv_obj_t* lbl_version;
bool status = true;

// State tracking for startup workflow
static bool stabilization_complete = false;
static bool calibration_loaded = false;

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
// Paste generated UI code here. The code should:
// - Create widgets as children of 'scr' parameter
// - Assign widgets that need updating to the static variables above
//
static void build_screen_content(lv_obj_t* scr) {
    // Title label
    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "PPO DISPLAY");
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 40);

    // Status label (dynamic)
    lbl_status = lv_label_create(scr);
    lv_label_set_text(lbl_status, "Auto calibrating on air");
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, 20);

    // Version info
    lbl_version = lv_label_create(scr);
    lv_label_set_text(lbl_version, status_buf1);
    lv_obj_set_style_text_color(lbl_version, lv_color_make(128, 128, 128), 0);
    lv_obj_set_style_text_font(lbl_version, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_version, LV_ALIGN_BOTTOM_MID, 0, -20);
}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the startup screen
 */
lv_obj_t* screen_startup_create(void) {
    // Reset state when screen is created
    startup_ticks = 0;
    lbl_status = NULL;
    stabilization_complete = false;
    calibration_loaded = false;

    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Build screen content
    build_screen_content(scr);

    // Initial message - waiting for sensor stabilization
    lv_label_set_text(lbl_status, "Waiting sensor stabilisation");

    return scr;

}

/**
 * @brief Called when sensor stabilization completes
 */
void screen_startup_on_stabilization_complete(void) {
    stabilization_complete = true;

    // Sensors are stable - load calibration from flash
    if (flash_calibration_load()) {
        calibration_loaded = true;
        if (lbl_status) {
            lv_label_set_text(lbl_status, "Calibration loaded");
        }
    } else {
        calibration_loaded = false;
        if (lbl_status) {
            lv_label_set_text(lbl_status, "No calibration - starting setup");
        }
    }
}

/**
 * @brief Periodic update - called at 1Hz while this screen is active
 */
void screen_startup_update(void) {
    if (!stabilization_complete) {
        // Still waiting for sensor stabilization
        // Toggle status indicator
        if (status) {
            lv_label_set_text(lbl_version, status_buf1);
        } else {
            lv_label_set_text(lbl_version, status_buf2);
        }
        status = !status;
        return;
    }

    // Stabilization complete - handle calibration state
    if (calibration_loaded) {
        // Calibration exists - auto-advance after 2 seconds
        startup_ticks++;
        if (startup_ticks >= 2) {
            screen_manager_switch(SCREEN_MAIN);
        }
    } else {
        // No calibration - auto-switch to calibration screen after 1 second
        startup_ticks++;
        if (startup_ticks >= 1) {
            screen_manager_switch(SCREEN_CALIBRATION);
        }
    }
}
