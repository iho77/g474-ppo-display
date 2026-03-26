/**
 * @file screen_menu.c
 * @brief Menu screen - Navigation hub
 *
 * Menu items:
 * 0. Calibration
 * 1. Sensor Data
 * 2. Setup
 * 3. Power Off
 * 4. <- Back (to Main)
 *
 * BTN_M cycles through items, BTN_S selects current item.
 */

#include "screen_manager.h"
#include "lvgl.h"

// ============================================================================
// PRIVATE CONSTANTS
// ============================================================================
#define MENU_ITEM_COUNT 6


static const screen_id_t menu_targets[MENU_ITEM_COUNT] = {
		 SCREEN_CALIBRATION,    // 0: Calibration
		 SCREEN_SENSOR_DATA,    // 1: Sensor data
		 SCREEN_DIVE_LOG,       // 2: Dive log
		 SCREEN_SETTINGS,       // 3: Setup
		 SCREEN_POWER_OFF,      // 4: Power off
         SCREEN_MAIN            // 5: Back to main
};

// ============================================================================
// PRIVATE VARIABLES
// ============================================================================
static int current_selection = 0;
static lv_obj_t* menu_labels[MENU_ITEM_COUNT] = {NULL};



lv_obj_t * ui_Label5 = NULL;
lv_obj_t * ui_Label7 = NULL;
lv_obj_t * ui_Label_divelog = NULL;
lv_obj_t * ui_Label9 = NULL;
lv_obj_t * ui_Label10 = NULL;
lv_obj_t * ui_Label11 = NULL;

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {

	    ui_Label5 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label5, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label5, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label5, 0);
	    lv_obj_set_y(ui_Label5, -87);
	    lv_obj_set_align(ui_Label5, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label5, "Calibration");
	    lv_obj_set_style_text_color(ui_Label5, lv_color_make(0, 255, 255), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label5, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label7 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label7, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label7, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label7, 0);
	    lv_obj_set_y(ui_Label7, -52);
	    lv_obj_set_align(ui_Label7, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label7, "Sensor data");
	    lv_obj_set_style_text_color(ui_Label7, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label7, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label_divelog = lv_label_create(scr);
	    lv_obj_set_width(ui_Label_divelog, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label_divelog, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label_divelog, 0);
	    lv_obj_set_y(ui_Label_divelog, -17);
	    lv_obj_set_align(ui_Label_divelog, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label_divelog, "Dive log");
	    lv_obj_set_style_text_color(ui_Label_divelog, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label_divelog, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label_divelog, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label9 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label9, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label9, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label9, 0);
	    lv_obj_set_y(ui_Label9, 18);
	    lv_obj_set_align(ui_Label9, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label9, "Setup");
	    lv_obj_set_style_text_color(ui_Label9, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label9, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label9, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label10 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label10, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label10, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label10, 0);
	    lv_obj_set_y(ui_Label10, 53);
	    lv_obj_set_align(ui_Label10, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label10, "Power off");
	    lv_obj_set_style_text_color(ui_Label10, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label10, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label11 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label11, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_Label11, LV_SIZE_CONTENT);
	    lv_obj_set_x(ui_Label11, 0);
	    lv_obj_set_y(ui_Label11, 88);
	    lv_obj_set_align(ui_Label11, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label11, "Back");
	    lv_obj_set_style_text_color(ui_Label11, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label11, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label11, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);


	    menu_labels[0] = ui_Label5;
	    menu_labels[1] = ui_Label7;
	    menu_labels[2] = ui_Label_divelog;
	    menu_labels[3] = ui_Label9;
	    menu_labels[4] = ui_Label10;
	    menu_labels[5] = ui_Label11;


}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the menu screen
 */
lv_obj_t* screen_menu_create(void) {
    // Preserve selection across screen transitions (static variable persists)
    // Only validate bounds in case of corruption
    if (current_selection >= MENU_ITEM_COUNT) {
        current_selection = 0;
    }

    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(scr, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Build screen content
    build_screen_content(scr);

    // Set proper highlighting: all white except current selection (cyan)
    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        if (menu_labels[i] != NULL) {
            if (i == current_selection) {
                lv_obj_set_style_text_color(menu_labels[i], lv_color_make(0, 255, 255), 0);
            } else {
                lv_obj_set_style_text_color(menu_labels[i], lv_color_white(), 0);
            }
        }
    }

    return scr;
}

// ============================================================================
// MENU-SPECIFIC API
// ============================================================================

/**
 * @brief Cycle to next menu item (BTN_M handler)
 */
void menu_cycle_selection(void) {
    if (menu_labels[0] == NULL) return;

    // Remove highlight from current item
    lv_obj_set_style_text_color(menu_labels[current_selection], lv_color_white(), 0);

    // Move to next item (wrap around)
    current_selection = (current_selection + 1) % MENU_ITEM_COUNT;

    // Highlight new item
    lv_obj_set_style_text_color(menu_labels[current_selection], lv_color_make(0, 255, 255), 0);
}

/**
 * @brief Select current menu item (BTN_S handler)
 */
void menu_select_current(void) {
    // Navigate to selected screen
    screen_manager_switch(menu_targets[current_selection]);
}

/**
 * @brief Get current selection index (for debugging)
 */
int menu_get_selection(void) {
    return current_selection;
}
