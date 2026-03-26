/**
 * @file screen_power_off.c
 * @brief Power Off screen - Shutdown confirmation
 *
 * Confirms user intent to power down the device.
 * BTN_M returns to menu, BTN_S confirms shutdown.
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "main.h"

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================
static lv_obj_t* lbl_status = NULL;
static lv_obj_t* lbl_instruction = NULL;
static bool shutdown_confirmed = false;  // Prevent re-entry

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {
    // Title label (red warning color)
    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_obj_set_align(lbl_title, LV_ALIGN_TOP_MID);
    lv_obj_set_y(lbl_title, 30);
    lv_label_set_text(lbl_title, "POWER OFF");
    lv_obj_set_style_text_color(lbl_title, lv_color_make(255, 100, 100), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);

    // Status/confirmation message (center)
    lbl_status = lv_label_create(scr);
    lv_obj_set_align(lbl_status, LV_ALIGN_CENTER);
    lv_obj_set_y(lbl_status, -10);
    lv_label_set_text(lbl_status, "Power off device?");
    lv_obj_set_style_text_color(lbl_status, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);

    // Instructions (bottom)
    lbl_instruction = lv_label_create(scr);
    lv_obj_set_align(lbl_instruction, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(lbl_instruction, -40);
    lv_label_set_text(lbl_instruction, "BTN_M: Cancel\nBTN_S: Confirm");
    lv_obj_set_style_text_color(lbl_instruction, lv_color_make(180, 180, 180), 0);
    lv_obj_set_style_text_align(lbl_instruction, LV_TEXT_ALIGN_CENTER, 0);
}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the power off screen
 */
lv_obj_t* screen_power_off_create(void) {
    // Reset shutdown confirmation flag
    shutdown_confirmed = false;

    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    // Build screen content
    build_screen_content(scr);

    return scr;
}

/**
 * @brief Action handler - called when BTN_S pressed
 */
void screen_power_off_action(btn_event_t evt) {
    (void)evt;  // Unused for now
    if (shutdown_confirmed) return;  // Prevent double-execution
    shutdown_confirmed = true;

    // Update UI
    if (lbl_status) lv_label_set_text(lbl_status, "Shutting down...");
    if (lbl_instruction) lv_label_set_text(lbl_instruction, "Please wait");

    // Force UI update and brief delay
    lv_timer_handler();
    HAL_Delay(500);

    // Execute shutdown (implemented in main.c)
    system_shutdown();
}
