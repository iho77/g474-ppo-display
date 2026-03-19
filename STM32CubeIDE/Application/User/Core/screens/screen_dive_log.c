/**
 * @file screen_dive_log.c
 * @brief Dive Log screen - Historical dive data
 *
 * Displays dive history, statistics, and log entries.
 * BTN_M returns to menu, BTN_S cycles through log entries.
 */

#include "screen_manager.h"
#include "lvgl.h"

// ============================================================================
// PRIVATE VARIABLES - Widget references for update() callback
// ============================================================================
// static lv_obj_t* lbl_log_entry = NULL;  // TODO: Implement dive log UI
static int current_log_index = 0;

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {

}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the dive log screen
 */
lv_obj_t* screen_dive_log_create(void) {
    // Reset to first log entry
    current_log_index = 0;

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
void screen_dive_log_action(btn_event_t evt) {
    (void)evt;  // Unused for now
    // TODO: Cycle through dive log entries
    // Example: load next log entry from storage and display

}
