/**
 * @file screen_manager.h
 * @brief Screen management framework for LVGL-based dive computer UI
 *
 * Factory Pattern: Creates/destroys screens on demand to minimize RAM usage.
 * Only ONE screen exists in memory at any time.
 */

#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include "lvgl.h"
#include "button.h"

#define CAL_AUTO "Cal Air"
#define CAL_AUTO_COL 0x000000
#define CAL_2pt_COL 0x056536
#define CAL_FLT_COL 0xF90625
#define CAL_2pt "Cal 2pt"
#define CAL_1pt "Cal 1pt"
#define FAULT "FAULT"



/**
 * @brief Screen identifiers for all application screens
 */
typedef enum {
    SCREEN_STARTUP = 0,      /**< Boot splash and initialization status */
    SCREEN_MAIN,             /**< Primary dive display (PPO2, etc.) */
    SCREEN_MENU,             /**< Navigation hub with selectable items */
    SCREEN_CALIBRATION,      /**< Sensor calibration workflow */
    SCREEN_SETTINGS,         /**< User preferences */
    SCREEN_SENSOR_DATA,      /**< Raw sensor readings and diagnostics */
    SCREEN_DIVE_LOG,         /**< Historical dive data */
    SCREEN_POWER_OFF,        /**< Shutdown confirmation */
    SCREEN_COUNT             /**< Total number of screens */
} screen_id_t;

/**
 * @brief Screen definition structure - factory pattern callbacks
 */
typedef struct {
    lv_obj_t* (*create)(void);         /**< Factory: creates and returns screen */
    void (*update)(void);               /**< Periodic update (called at 1Hz) */
    void (*on_action)(btn_event_t evt); /**< Button event handler (screen-specific) */
    const char* name;                   /**< Screen name for debugging */
} screen_def_t;

/**
 * @brief Initialize the screen manager and load the startup screen
 * @note Call once during system initialization before main loop
 */
void screen_manager_init(void);

/**
 * @brief Switch to a different screen
 * @param id Screen to switch to
 * @note Destroys current screen and creates new screen (factory pattern)
 */
void screen_manager_switch(screen_id_t id);

/**
 * @brief Route button events to the current screen
 * @param evt Button event to handle
 * @note Called from main loop when button_get_event() returns non-zero
 */
void screen_manager_handle_button(btn_event_t evt);

/**
 * @brief Periodic update for the current screen
 * @note Called at 1Hz from main loop - updates dynamic content
 */
void screen_manager_update(void);

/**
 * @brief Get the current active screen ID
 * @return Current screen identifier
 */
screen_id_t screen_manager_current(void);

/**
 * @brief Auto-transition from Startup to Main screen
 * @note Called when calibration completes
 */
void screen_manager_startup_complete(void);

// ============================================================================
// SCREEN FACTORY FUNCTION DECLARATIONS
// Each screen must implement these functions in screens/screen_<name>.c
// ============================================================================

// Startup screen
lv_obj_t* screen_startup_create(void);
void screen_startup_update(void);
void screen_startup_on_stabilization_complete(void);  // Called when sensors stabilize

// Main screen
lv_obj_t* screen_main_create(void);
void screen_main_update(void);
void screen_main_on_action(btn_event_t evt);

// Menu screen
lv_obj_t* screen_menu_create(void);
void menu_cycle_selection(void);    // BTN_M: move highlight to next item
void menu_select_current(void);     // BTN_S: enter selected screen or back

// Calibration screen
lv_obj_t* screen_calibration_create(void);
void screen_calibration_update(void);
void screen_calibration_action(void);

// Settings screen
lv_obj_t* screen_settings_create(void);
void screen_settings_action(void);

// Sensor Data screen
lv_obj_t* screen_sensor_data_create(void);
void screen_sensor_data_update(void);

// Dive Log screen
lv_obj_t* screen_dive_log_create(void);
void screen_dive_log_action(btn_event_t evt);

// Power Off screen
lv_obj_t* screen_power_off_create(void);
void screen_power_off_action(btn_event_t evt);

#endif // SCREEN_MANAGER_H
