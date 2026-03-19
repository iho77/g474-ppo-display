/**
 * @file screen_manager.c
 * @brief Screen management implementation - factory pattern with on-demand creation
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "button.h"

// Forward declarations for custom button handlers
void screen_main_on_action(btn_event_t evt);
void screen_calibration_on_button(btn_event_t evt);
void screen_settings_on_button(btn_event_t evt);

// ============================================================================
// SCREEN REGISTRY - Factory function table
// ============================================================================
static const screen_def_t screen_defs[SCREEN_COUNT] = {
    [SCREEN_STARTUP]     = { screen_startup_create,     screen_startup_update,     NULL,                      "Startup" },
    [SCREEN_MAIN]        = { screen_main_create,        screen_main_update,        screen_main_on_action,     "Main" },
    [SCREEN_MENU]        = { screen_menu_create,        NULL,                      NULL,                      "Menu" },
    [SCREEN_CALIBRATION] = { screen_calibration_create, screen_calibration_update, NULL,                      "Calibration" },
    [SCREEN_SETTINGS]    = { screen_settings_create,    NULL,                      NULL,                      "Settings" },
    [SCREEN_SENSOR_DATA] = { screen_sensor_data_create, screen_sensor_data_update, NULL,                      "Sensor Data" },
    [SCREEN_DIVE_LOG]    = { screen_dive_log_create,    NULL,                      screen_dive_log_action,    "Dive Log" },
    [SCREEN_POWER_OFF]   = { screen_power_off_create,   NULL,                      screen_power_off_action,   "Power Off" },
};

// ============================================================================
// PRIVATE STATE
// ============================================================================
static screen_id_t current_screen = SCREEN_STARTUP;
static lv_obj_t* current_screen_obj = NULL;

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void screen_manager_init(void) {
    // Load the startup screen
    current_screen = SCREEN_STARTUP;
    current_screen_obj = NULL;
    screen_manager_switch(SCREEN_STARTUP);
}

void screen_manager_switch(screen_id_t id) {
    // Validate screen ID
    if (id >= SCREEN_COUNT) {
        return;
    }

    // Skip if already on this screen
    if (id == current_screen && current_screen_obj != NULL) {
        return;
    }

    // Save reference to old screen and screen ID for rollback on failure
    lv_obj_t* old_screen = current_screen_obj;
    screen_id_t old_screen_id = current_screen;

    // 1. CREATE new screen FIRST (allocate from LVGL pool)
    current_screen = id;
    if (screen_defs[id].create != NULL) {
        current_screen_obj = screen_defs[id].create();

        // CRITICAL: Validate screen creation succeeded
        if (current_screen_obj == NULL) {
            // LVGL memory pool exhausted - cannot allocate new screen
            // Restore previous screen state to maintain consistent UI
            current_screen = old_screen_id;
            current_screen_obj = old_screen;  // Keep old screen active

            // TODO: Consider logging error or displaying warning to user
            // For now, silently stay on current screen (fail-safe)
            return;
        }

        // 2. LOAD new screen (v8 API - makes it active immediately without animation)
        //    lv_scr_load_anim() automatically invalidates and triggers refresh
        lv_scr_load_anim(current_screen_obj, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);

        // 3. DESTROY old screen AFTER new one is loaded (free LVGL objects back to pool)
        if (old_screen != NULL) {
            lv_obj_del(old_screen);  // v8 API - Recursively frees all children
        }

        // 4. CLEAR pending button events to prevent stale events from affecting new screen
        //    This prevents button bounce during screen transition from triggering actions on new screen
        button_clear_event();
    }
}

void screen_manager_handle_button(btn_event_t evt) {
    if (evt == BTN_NONE) {
        return;
    }

    switch (current_screen) {
        case SCREEN_STARTUP:
            // Ignore buttons during startup - auto-advances after calibration
            break;

        case SCREEN_MAIN:
            // Route all button events to main screen handler
            // Handler will decide whether to enter menu (BTN_M in normal mode)
            // or handle setpoint selection (BTN_S or BTN_M in selection mode)
            if (screen_defs[current_screen].on_action != NULL) {
                screen_defs[current_screen].on_action(evt);
            }
            break;

        case SCREEN_MENU:
            if (evt == BTN_M_PRESS) {
                menu_cycle_selection();  // Highlight next menu item
            } else if (evt == BTN_S_PRESS) {
                menu_select_current();   // Enter sub-screen or back to Main
            }
            break;

        case SCREEN_CALIBRATION:
            // Calibration screen needs custom button handling
            screen_calibration_on_button(evt);
            break;

        case SCREEN_SETTINGS:
            // Settings screen needs custom button handling
            screen_settings_on_button(evt);
            break;

        default:  // Other sub-screens (Sensor Data, etc.)
            if (evt == BTN_M_PRESS) {
                screen_manager_switch(SCREEN_MENU);  // Back to menu
            } else if (evt == BTN_S_PRESS) {
                // Call screen-specific action handler (if defined)
                if (screen_defs[current_screen].on_action != NULL) {
                    screen_defs[current_screen].on_action(evt);
                }
            }
            break;
    }
}

void screen_manager_update(void) {
    // Call the current screen's update function (if defined)
    if (screen_defs[current_screen].update != NULL) {
        screen_defs[current_screen].update();
    }
}

screen_id_t screen_manager_current(void) {
    return current_screen;
}

void screen_manager_startup_complete(void) {
    // Called when sensor stabilization completes
    if (current_screen == SCREEN_STARTUP) {
        // Notify startup screen to load calibration
        screen_startup_on_stabilization_complete();
    }
}
