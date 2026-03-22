/**
 * @file screen_main.c
 * @brief Main dive display screen - Primary PPO2 readings
 *
 * Displays sensor PPO2 values, battery status, and dive information.
 * Press BTN_M to enter menu.
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "sensor.h"
#include "warning.h"
#include "drift_tracker.h"
#include "pressure_sensor.h"
#include <stdio.h>
#include <stdlib.h>

// External elapsed time counter (defined in main.c)
extern uint32_t g_elapsed_time_sec;

#define SENSOR1 0
#define SENSOR2 1
#define SENSOR3 2

#define BRD_LBL 0
#define PPO_LBL 1
#define UV_LBL 2

// Setpoint selection state machine
#define SELECTION_NORMAL      0
#define SELECTION_SELECTING   1
#define SELECTION_TIMEOUT_SEC 30

static uint8_t selection_state = SELECTION_NORMAL;
static uint8_t selection_timer = 0;  // Seconds in SELECTING mode


// External font declaration (defined in Drivers/lvgl/src/font/lv_font_montserrat_28_new.c)
LV_FONT_DECLARE(lv_font_montserrat_28_new);
LV_FONT_DECLARE(lv_font_montserrat_40_new);
// ============================================================================
// PRIVATE VARIABLES - Widget references for update() callback
// ============================================================================

lv_obj_t * ui_lblDepth = NULL;
lv_obj_t * ui_lblPPOtext = NULL;
lv_obj_t * ui_lblDepthValue = NULL;
lv_obj_t * ui_lblS1V = NULL;
lv_obj_t * ui_lblS2V = NULL;
lv_obj_t * ui_lblS3V = NULL;
lv_obj_t * ui_lbBat = NULL;
lv_obj_t * ui_lblSV1 = NULL;
lv_obj_t * ui_Label12 = NULL;
lv_obj_t * ui_lbSP1 = NULL;
lv_obj_t * ui_lbSP2 = NULL;
lv_obj_t * ui_lbElapsedTime = NULL;  // Elapsed time timer label
lv_obj_t * ui_Label15 = NULL;
lv_obj_t * ui_dPPO = NULL;
lv_obj_t * ui_Label16 = NULL;
lv_obj_t * ui_lbCP2 = NULL;
lv_obj_t * ui_lbCP1 = NULL;
lv_obj_t * ui_Label19 = NULL;
lv_obj_t * ui_Label20 = NULL;
lv_obj_t * ui_lbdT = NULL;
lv_obj_t * ui_Label22 = NULL;
lv_obj_t * ui_Label23 = NULL;
lv_obj_t * ui_lbTTL = NULL;
lv_obj_t * ui_Label25 = NULL;
lv_obj_t * ui_Panel3 = NULL;
lv_obj_t * ui_lblS1PPO = NULL;
lv_obj_t * ui_Panel2 = NULL;
lv_obj_t * ui_lblS2PPO = NULL;
lv_obj_t * ui_Panel4 = NULL;
lv_obj_t * ui_lblS3PPO = NULL;



lv_obj_t * display_data[3][3];

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
// Paste generated UI code here. The code should:
// - Create widgets as children of 'scr' parameter
// - Assign widgets that need updating to the static variables above
//
static void build_screen_content(lv_obj_t* scr){

	    ui_lblDepth = lv_label_create(scr);
	    lv_obj_set_width(ui_lblDepth, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblDepth, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblDepth, 15);
	    lv_obj_set_y(ui_lblDepth, 6);
	    lv_label_set_text(ui_lblDepth, "D:");
	    lv_obj_set_style_text_color(ui_lblDepth, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblDepth, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblDepth, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblPPOtext = lv_label_create(scr);
	    lv_obj_set_width(ui_lblPPOtext, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblPPOtext, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblPPOtext, -113);
	    lv_obj_set_y(ui_lblPPOtext, 23);
	    lv_obj_set_align(ui_lblPPOtext, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lblPPOtext, "Depth");
	    lv_obj_set_style_text_color(ui_lblPPOtext, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblPPOtext, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblPPOtext, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblDepthValue = lv_label_create(scr);
	    lv_obj_set_width(ui_lblDepthValue, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblDepthValue, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblDepthValue, -110);
	    lv_obj_set_y(ui_lblDepthValue, 54);
	    lv_obj_set_align(ui_lblDepthValue, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lblDepthValue, "---");
	    lv_obj_set_style_text_color(ui_lblDepthValue, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblDepthValue, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblDepthValue, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS1V = lv_label_create(scr);
	    lv_obj_set_width(ui_lblS1V, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS1V, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblS1V, -106);
	    lv_obj_set_y(ui_lblS1V, 43);
	    lv_obj_set_align(ui_lblS1V, LV_ALIGN_TOP_MID);
	    lv_label_set_text(ui_lblS1V, "10.1v");
	    lv_obj_set_style_text_color(ui_lblS1V, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS1V, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS1V, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS2V = lv_label_create(scr);
	    lv_obj_set_width(ui_lblS2V, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS2V, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblS2V, 0);
	    lv_obj_set_y(ui_lblS2V, 43);
	    lv_obj_set_align(ui_lblS2V, LV_ALIGN_TOP_MID);
	    lv_label_set_text(ui_lblS2V, "10.1v");
	    lv_obj_set_style_text_color(ui_lblS2V, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS2V, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS2V, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS3V = lv_label_create(scr);
	    lv_obj_set_width(ui_lblS3V, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS3V, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblS3V, 99);
	    lv_obj_set_y(ui_lblS3V, 43);
	    lv_obj_set_align(ui_lblS3V, LV_ALIGN_TOP_MID);
	    lv_label_set_text(ui_lblS3V, "0v");
	    lv_obj_set_style_text_color(ui_lblS3V, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS3V, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS3V, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbBat = lv_label_create(scr);
	    lv_obj_set_width(ui_lbBat, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbBat, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbBat, 110);
	    lv_obj_set_y(ui_lbBat, 4);
	    lv_obj_set_align(ui_lbBat, LV_ALIGN_TOP_MID);
	    lv_label_set_text(ui_lbBat, "90%");
	    lv_obj_set_style_text_color(ui_lbBat, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbBat, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbBat, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblSV1 = lv_label_create(scr);
	    lv_obj_set_width(ui_lblSV1, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblSV1, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lblSV1, -407);
	    lv_obj_set_y(ui_lblSV1, 265);
	    lv_obj_set_align(ui_lblSV1, LV_ALIGN_TOP_MID);
	    lv_label_set_text(ui_lblSV1, "10.1v");
	    lv_obj_set_style_text_color(ui_lblSV1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblSV1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblSV1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label12 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label12, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label12, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label12, -123);
	    lv_obj_set_y(ui_Label12, 102);
	    lv_obj_set_align(ui_Label12, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label12, "SP1");
	    lv_obj_set_style_text_color(ui_Label12, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label12, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbSP1 = lv_label_create(scr);
	    lv_obj_set_width(ui_lbSP1, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbSP1, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbSP1, -92);
	    lv_obj_set_y(ui_lbSP1, 102);
	    lv_obj_set_align(ui_lbSP1, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbSP1, "1.6");
	    lv_obj_set_style_text_color(ui_lbSP1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbSP1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbSP1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbSP2 = lv_label_create(scr);
	    lv_obj_set_width(ui_lbSP2, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbSP2, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbSP2, 132);
	    lv_obj_set_y(ui_lbSP2, 103);
	    lv_obj_set_align(ui_lbSP2, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbSP2, "1.2");
	    lv_obj_set_style_text_color(ui_lbSP2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbSP2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbSP2, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    // Elapsed time timer (centered between SP1 and SP2)
	    ui_lbElapsedTime = lv_label_create(scr);
	    lv_obj_set_width(ui_lbElapsedTime, LV_SIZE_CONTENT);
	    lv_obj_set_height(ui_lbElapsedTime, LV_SIZE_CONTENT);
	    lv_obj_set_align(ui_lbElapsedTime, LV_ALIGN_CENTER);
	    lv_obj_set_x(ui_lbElapsedTime, 0);     // Screen center (x=160)
	    lv_obj_set_y(ui_lbElapsedTime, 102);   // Same baseline as SP1/SP2
	    lv_obj_set_style_text_color(ui_lbElapsedTime, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbElapsedTime, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbElapsedTime, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_label_set_text(ui_lbElapsedTime, "00:00:00");

	    ui_Label15 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label15, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label15, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label15, 101);
	    lv_obj_set_y(ui_Label15, 103);
	    lv_obj_set_align(ui_Label15, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label15, "SP2");
	    lv_obj_set_style_text_color(ui_Label15, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_dPPO = lv_label_create(scr);
	    lv_obj_set_width(ui_dPPO, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_dPPO, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_dPPO, -105);
	    lv_obj_set_y(ui_dPPO, -106);
	    lv_obj_set_align(ui_dPPO, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_dPPO, "0.001");
	    lv_obj_set_style_text_color(ui_dPPO, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_dPPO, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_dPPO, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_opa(ui_dPPO, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label16 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label16, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label16, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label16, -50);
	    lv_obj_set_y(ui_Label16, -107);
	    lv_obj_set_align(ui_Label16, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label16, "CP1:");
	    lv_obj_set_style_text_color(ui_Label16, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label16, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label16, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label19 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label19, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label19, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label19, 15);
	    lv_obj_set_y(ui_Label19, -107);
	    lv_obj_set_align(ui_Label19, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label19, "CP2:");
	    lv_obj_set_style_text_color(ui_Label19, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label19, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label19, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbCP1 = lv_label_create(scr);
	    lv_obj_set_width(ui_lbCP1, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbCP1, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbCP1, -18);
	    lv_obj_set_y(ui_lbCP1, -107);
	    lv_obj_set_align(ui_lbCP1, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbCP1, "0.00");
	    lv_obj_set_style_text_color(ui_lbCP1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbCP1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbCP1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbCP2 = lv_label_create(scr);
	    lv_obj_set_width(ui_lbCP2, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbCP2, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbCP2, 47);
	    lv_obj_set_y(ui_lbCP2, -107);
	    lv_obj_set_align(ui_lbCP2, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbCP2, "0.21");
	    lv_obj_set_style_text_color(ui_lbCP2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbCP2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbCP2, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label20 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label20, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label20, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label20, -3);
	    lv_obj_set_y(ui_Label20, 23);
	    lv_obj_set_align(ui_Label20, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label20, "dPPO");
	    lv_obj_set_style_text_color(ui_Label20, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label20, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label20, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbdT = lv_label_create(scr);
	    lv_obj_set_width(ui_lbdT, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbdT, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbdT, 3);
	    lv_obj_set_y(ui_lbdT, 49);
	    lv_obj_set_align(ui_lbdT, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbdT, "0.001");
	    lv_obj_set_style_text_color(ui_lbdT, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbdT, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbdT, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label22 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label22, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label22, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label22, 1);
	    lv_obj_set_y(ui_Label22, 79);
	    lv_obj_set_align(ui_Label22, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label22, "mbar/min");
	    lv_obj_set_style_text_color(ui_Label22, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label22, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label22, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label23 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label23, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label23, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label23, 113);
	    lv_obj_set_y(ui_Label23, 25);
	    lv_obj_set_align(ui_Label23, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label23, "TTL");
	    lv_obj_set_style_text_color(ui_Label23, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label23, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label23, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lbTTL = lv_label_create(scr);
	    lv_obj_set_width(ui_lbTTL, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lbTTL, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_lbTTL, 111);
	    lv_obj_set_y(ui_lbTTL, 52);
	    lv_obj_set_align(ui_lbTTL, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lbTTL, "12");
	    lv_obj_set_style_text_color(ui_lbTTL, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lbTTL, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lbTTL, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Label25 = lv_label_create(scr);
	    lv_obj_set_width(ui_Label25, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_Label25, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_x(ui_Label25, 112);
	    lv_obj_set_y(ui_Label25, 80);
	    lv_obj_set_align(ui_Label25, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_Label25, "min");
	    lv_obj_set_style_text_color(ui_Label25, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_Label25, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_Label25, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Panel3 = lv_obj_create(scr);
	    lv_obj_set_width(ui_Panel3, 100);
	    lv_obj_set_height(ui_Panel3, 48);
	    lv_obj_set_x(ui_Panel3, -102);
	    lv_obj_set_y(ui_Panel3, -20);
	    lv_obj_set_align(ui_Panel3, LV_ALIGN_CENTER);
	    lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	    lv_obj_set_style_bg_color(ui_Panel3, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_opa(ui_Panel3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_color(ui_Panel3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_opa(ui_Panel3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_width(ui_Panel3, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS1PPO = lv_label_create(ui_Panel3);
	    lv_obj_set_width(ui_lblS1PPO, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS1PPO, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_align(ui_lblS1PPO, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lblS1PPO, "0.00");
	    lv_obj_set_style_text_color(ui_lblS1PPO, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS1PPO, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS1PPO, &lv_font_montserrat_40_new, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Panel2 = lv_obj_create(scr);
	    lv_obj_set_width(ui_Panel2, 100);
	    lv_obj_set_height(ui_Panel2, 48);
	    lv_obj_set_x(ui_Panel2, 3);
	    lv_obj_set_y(ui_Panel2, -20);
	    lv_obj_set_align(ui_Panel2, LV_ALIGN_CENTER);
	    lv_obj_clear_flag(ui_Panel2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	    lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_opa(ui_Panel2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_color(ui_Panel2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_opa(ui_Panel2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_width(ui_Panel2, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS2PPO = lv_label_create(ui_Panel2);
	    lv_obj_set_width(ui_lblS2PPO, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS2PPO, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_align(ui_lblS2PPO, LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lblS2PPO, "0.00");
	    lv_obj_set_style_text_color(ui_lblS2PPO, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS2PPO, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS2PPO, &lv_font_montserrat_40_new, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_Panel4 = lv_obj_create(scr);
	    lv_obj_set_width(ui_Panel4, 100);
	    lv_obj_set_height(ui_Panel4, 48);
	    lv_obj_set_x(ui_Panel4, 107);
	    lv_obj_set_y(ui_Panel4, -20);
	    lv_obj_set_align(ui_Panel4, LV_ALIGN_CENTER);
	    lv_obj_clear_flag(ui_Panel4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	    lv_obj_set_style_bg_color(ui_Panel4, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_bg_opa(ui_Panel4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_color(ui_Panel4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_opa(ui_Panel4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_border_width(ui_Panel4, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

	    ui_lblS3PPO = lv_label_create(ui_Panel4);
	    lv_obj_set_width(ui_lblS3PPO, LV_SIZE_CONTENT);   /// 1
	    lv_obj_set_height(ui_lblS3PPO, LV_SIZE_CONTENT);    /// 1
	    lv_obj_set_align(ui_lblS3PPO,LV_ALIGN_CENTER);
	    lv_label_set_text(ui_lblS3PPO, "0.00");
	    lv_obj_set_style_text_color(ui_lblS3PPO, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_opa(ui_lblS3PPO, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	    lv_obj_set_style_text_font(ui_lblS3PPO, &lv_font_montserrat_40_new, LV_PART_MAIN | LV_STATE_DEFAULT);


	    display_data[SENSOR1][PPO_LBL] = ui_lblS1PPO;
	    display_data[SENSOR2][PPO_LBL] = ui_lblS2PPO;
	    display_data[SENSOR3][PPO_LBL] = ui_lblS3PPO;
	    display_data[SENSOR1][UV_LBL] = ui_lblS1V;
	    display_data[SENSOR2][UV_LBL] = ui_lblS2V;
	    display_data[SENSOR3][UV_LBL] = ui_lblS3V;
	    display_data[SENSOR1][BRD_LBL] = ui_Panel3;
	    display_data[SENSOR2][BRD_LBL] = ui_Panel2;
	    display_data[SENSOR3][BRD_LBL] = ui_Panel4;
}


// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Update setpoint background colors based on mode and active setpoint
 * @note NORMAL mode: Active has green, inactive has none
 * @note SELECTING mode: Active has green, alternative has yellow
 * @note Only highlights label text (SP1/SP2), not the values for readability
 */
static void update_setpoint_indicators(void) {
    // Clear all backgrounds first (only labels, not values)
    lv_obj_set_style_bg_opa(ui_Label12, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Label15, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);

    if (selection_state == SELECTION_SELECTING) {
        // SELECTING mode: Show both green (active) and yellow (alternative)
        if (active_setpoint == 1) {
            // SP1 active (green), SP2 alternative (yellow)
            lv_obj_set_style_bg_color(ui_Label12, lv_color_hex(0x056536), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label12, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(ui_Label15, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label15, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else { // active_setpoint == 2
            // SP2 active (green), SP1 alternative (yellow)
            lv_obj_set_style_bg_color(ui_Label15, lv_color_hex(0x056536), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label15, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

            lv_obj_set_style_bg_color(ui_Label12, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label12, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        // NORMAL mode: Show only active setpoint in green
        if (active_setpoint == 1) {
            lv_obj_set_style_bg_color(ui_Label12, lv_color_hex(0x056536), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label12, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        } else { // active_setpoint == 2
            lv_obj_set_style_bg_color(ui_Label15, lv_color_hex(0x056536), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_Label15, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the main screen
 */
lv_obj_t* screen_main_create(void) {
    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
	lv_obj_clear_flag(scr, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
	lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(scr, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Build screen content
    build_screen_content(scr);

    // Initialize setpoint displays from settings
    static char buf[8];
    (void)snprintf(buf, sizeof(buf), "%d.%02d",
             sensor_data.settings.ppo_setpoint1 / 1000,
             (sensor_data.settings.ppo_setpoint1 % 1000) / 10);
    lv_label_set_text(ui_lbSP1, buf);

    (void)snprintf(buf, sizeof(buf), "%d.%02d",
             sensor_data.settings.ppo_setpoint2 / 1000,
             (sensor_data.settings.ppo_setpoint2 % 1000) / 10);
    lv_label_set_text(ui_lbSP2, buf);

    // Reset selection state on screen creation
    selection_state = SELECTION_NORMAL;
    selection_timer = 0;

    // Initial green indicator will be set by first screen_main_update() call

    return scr;
}

/**
 * @brief Periodic update - called at 1Hz while this screen is active
 */
void screen_main_update(void) {
    // SAFETY: Abort update if screen objects deallocated (during screen transition)
    // This prevents HardFault if screen_manager_switch() deletes widgets mid-update
    if (display_data[0][0] == NULL || ui_dPPO == NULL || ui_lbdT == NULL || ui_lbTTL == NULL) {
        return;  // Screen being destroyed, skip this update cycle
    }

    char buf[20];
    // Compute S1-S2 delta outside the loop with explicit validity guards
    int32_t delta_ppo = 0;
    bool delta_valid = sensor_data.s_valid[SENSOR1] && sensor_data.s_valid[SENSOR2];
    if (delta_valid) {
        delta_ppo = sensor_data.o2_s_ppo[SENSOR1] - sensor_data.o2_s_ppo[SENSOR2];
    }

    // Loop through all 3 sensors — collect highest warning level for vibro
    warning_level_t highest_level = WARNING_NONE;
    for (int i = 0; i < 3; i++) {
        if (sensor_data.s_valid[i]) {

            // Update PPO value (o2_s_ppo is in mbar, e.g., 210 mbar = 0.21 PPO)
            int32_t ppo = sensor_data.o2_s_ppo[i];
            if (ppo < 0) {
                lv_label_set_text(display_data[i][PPO_LBL], "---");
            } else {
                (void)snprintf(buf, sizeof(buf), "%d.%02d", (int)(ppo / 1000), (int)((ppo % 1000 + 5) / 10));
                lv_label_set_text(display_data[i][PPO_LBL], buf);
            }

            // Check warning level and apply styling
            warning_level_t level = warning_check_sensor(i);
            warning_apply_style(i, level,
                               display_data[i][BRD_LBL],  // Panel for border
                               display_data[i][PPO_LBL]); // Label for text/bg

            if (level > highest_level) highest_level = level;

            // Update voltage in millivolts (assuming o2_s_uv is in microvolts)
            int32_t uv = sensor_data.o2_s_uv[i];
            if (uv < 0) {
                lv_label_set_text(display_data[i][UV_LBL], "---");
            } else {
                (void)snprintf(buf, sizeof(buf), "%d.%02dmv", (int)(uv / 100), (int)(uv % 100));
                lv_obj_set_style_text_color(display_data[i][UV_LBL], lv_color_hex(0xFFFFFF),
                    LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_label_set_text(display_data[i][UV_LBL], buf);
            }

        } else {
            // Sensor invalid - show error state
        	lv_obj_set_style_border_color(display_data[i][BRD_LBL], lv_color_hex(CAL_FLT_COL), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(display_data[i][PPO_LBL], lv_color_hex(CAL_FLT_COL), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(display_data[i][UV_LBL], lv_color_hex(CAL_FLT_COL), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(display_data[i][PPO_LBL], "9.99");
            lv_label_set_text(display_data[i][UV_LBL], "err");
        }
    }
    // Trigger vibro for the most severe active warning across all sensors
    warning_trigger_vibration(highest_level);

    // Display delta PPO with absolute value (avoid minus sign not in font)
    // Clamp to sane PPO range before abs to avoid INT32_MIN UB
    buf[0] = '\0';
    int32_t clamped_delta = delta_ppo;
    if (clamped_delta < -9999) clamped_delta = -9999;
    if (clamped_delta >  9999) clamped_delta =  9999;
    int32_t abs_delta = (clamped_delta < 0) ? -clamped_delta : clamped_delta;
    (void)snprintf(buf, sizeof(buf), "%d.%03d", (int)(abs_delta / 1000), (int)(abs_delta % 1000));
    lv_label_set_text(ui_dPPO, buf);

    if (abs_delta > 20) {
    	lv_obj_set_style_bg_color(ui_dPPO, lv_color_hex(CAL_FLT_COL), LV_PART_MAIN  | LV_STATE_DEFAULT);
    } else {
    	lv_obj_set_style_bg_color(ui_dPPO, lv_color_hex(CAL_2pt_COL), LV_PART_MAIN  | LV_STATE_DEFAULT);
    }

    buf[0] = '\0';
    (void)snprintf(buf, sizeof(buf), "%d.%d%d",
        	      sensor_data.s_cpt[0].ppo / 1000,           // integer part
        	      (sensor_data.s_cpt[0].ppo / 100) % 10,     // first decimal
        	      (sensor_data.s_cpt[0].ppo / 10) % 10);
    lv_label_set_text(ui_lbCP1, buf);

    buf[0] = '\0';
    (void)snprintf(buf, sizeof(buf), "%d.%d%d",
    	      sensor_data.s_cpt[1].ppo / 1000,           // integer part
    	      (sensor_data.s_cpt[1].ppo / 100) % 10,     // first decimal
    	      (sensor_data.s_cpt[1].ppo / 10) % 10);
    lv_label_set_text(ui_lbCP2, buf);

    // ========================================================================
    // DEPTH DISPLAY - Show depth in meters with 1 decimal place
    // ========================================================================
    buf[0] = '\0';
    if (sensor_data.pressure.pressure_valid) {
        // Calculate depth from pressure difference
        int32_t depth_mm = pressure_sensor_calculate_depth_mm(
            sensor_data.pressure.pressure_mbar,
            sensor_data.pressure.surface_pressure_mbar
        );

        // Update depth in sensor data for external access
        sensor_data.pressure.depth_mm = depth_mm;

        // Convert mm to meters with 1 decimal place
        int32_t depth_m = depth_mm / 1000;
        int32_t depth_dm = (depth_mm % 1000) / 100;

        // Handle negative depth (above sea level / altitude diving)
        if (depth_m >= 0) {
            (void)snprintf(buf, sizeof(buf), "%ld.%01ld", (long)depth_m, (long)depth_dm);
        } else {
            // For negative depths, take absolute value for display
            (void)snprintf(buf, sizeof(buf), "-%ld.%01ld", (long)(-depth_m), (long)(-depth_dm));
        }
        lv_label_set_text(ui_lblDepthValue, buf);
    } else {
        // Invalid pressure data - show placeholder
        lv_label_set_text(ui_lblDepthValue, "---");
    }

    // ========================================================================
    // DRIFT RATE DISPLAY - Maximum absolute drift across all valid sensors
    // ========================================================================
    int32_t max_abs_drift = 0;
    int32_t max_drift_signed = 0;
    bool has_valid_drift = false;

    // Protect against SENSOR_COUNT corruption (EMI/bit-flip)
    int num_sensors = (SENSOR_COUNT >= 2) ? (SENSOR_COUNT - 2) : 0;
    if (num_sensors > 3) {  // Sanity check: we have exactly 3 O2 sensors
        num_sensors = 3;
    }

    for (int i = 0; i < num_sensors; i++) {
        if (sensor_data.s_valid[i] && sensor_data.drift[i].sample_count >= 3) {
            int32_t drift_rate = drift_get_rate_per_min(&sensor_data.drift[i]);
            int32_t abs_drift = (drift_rate < 0) ? -drift_rate : drift_rate;

            if (abs_drift > max_abs_drift) {
                max_abs_drift = abs_drift;
                max_drift_signed = drift_rate;  // Keep original sign
            }
            has_valid_drift = true;
        }
    }

    // Update drift display label (ui_lbdT)
    if (has_valid_drift) {
        // Display drift rate in mbar/min (no conversion needed - already in mbar)
        // Format: "+15" or "-12" (cleaner than "+0.015" bar)
        buf[0] = '\0';
        (void)snprintf(buf, sizeof(buf), "%+d", (int)max_drift_signed);
        lv_label_set_text(ui_lbdT, buf);

        // Color coding based on magnitude
        if (max_abs_drift > 50) {
            lv_obj_set_style_text_color(ui_lbdT, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);  // Red
        } else if (max_abs_drift > 20) {
            lv_obj_set_style_text_color(ui_lbdT, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);  // Yellow
        } else {
            lv_obj_set_style_text_color(ui_lbdT, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);  // White
        }
    } else {
        // Not enough samples yet - show placeholder
        lv_label_set_text(ui_lbdT, "---");
        lv_obj_set_style_text_color(ui_lbdT, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);  // Gray
    }

    // ========================================================================
    // TIME TO LIMIT (TTL) CALCULATION
    // ========================================================================
    // Calculate minimum time (in minutes) until any sensor reaches safety limits
    // based on current drift rate. Shows worst-case scenario across all sensors.
    int32_t min_ttl = INT32_MAX;  // Initialize to max value
    bool has_valid_ttl = false;

    // Protect against SENSOR_COUNT corruption (EMI/bit-flip) - reuse same bounds check
    // num_sensors already calculated above for drift display

    for (int i = 0; i < num_sensors; i++) {
        if (sensor_data.s_valid[i] && sensor_data.drift[i].sample_count >= 3) {
            int32_t drift_rate = drift_get_rate_per_min(&sensor_data.drift[i]);
            int32_t current_ppo = sensor_data.o2_s_ppo[i];
            int32_t ttl_minutes = INT32_MAX;

            if (drift_rate > 5) {
                // Positive drift - calculate time to max limit
                int32_t max_limit = sensor_data.settings.max_ppo_threshold;
                if (current_ppo < max_limit) {
                    ttl_minutes = (max_limit - current_ppo) / drift_rate;
                }
            } else if (drift_rate < -5) {
                // Negative drift - calculate time to min limit
                int32_t min_limit = sensor_data.settings.min_ppo_threshold;
                if (current_ppo > min_limit) {
                    ttl_minutes = (current_ppo - min_limit) / (-drift_rate);
                }
            }
            // If drift is between -5 and +5 mbar/min, skip (near zero, no meaningful TTL)

            if (ttl_minutes < min_ttl && ttl_minutes > 0) {
                min_ttl = ttl_minutes;
                has_valid_ttl = true;
            }
        }
    }

    // Update TTL display
    if (has_valid_ttl && min_ttl < 9999) {
        buf[0] = '\0';
        (void)snprintf(buf, sizeof(buf), "%d", (int)min_ttl);
        lv_label_set_text(ui_lbTTL, buf);

        // Red background if TTL < 2 minutes (critical warning)
        if (min_ttl < 2) {
            lv_obj_set_style_bg_color(ui_lbTTL, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(ui_lbTTL, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(ui_lbTTL, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);  // White text on red
        } else {
            // Normal display - transparent background
            lv_obj_set_style_bg_opa(ui_lbTTL, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(ui_lbTTL, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    } else {
        // Drift near zero or no valid data - show "--"
        lv_label_set_text(ui_lbTTL, "--");
        lv_obj_set_style_bg_opa(ui_lbTTL, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lbTTL, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);  // Gray
    }
    // ========================================================================

    // Handle selection timeout (30 seconds)
    if (selection_state == SELECTION_SELECTING) {
        selection_timer++;
        if (selection_timer >= SELECTION_TIMEOUT_SEC) {
            // Timeout - exit SELECTING mode without changing active setpoint
            selection_state = SELECTION_NORMAL;
            selection_timer = 0;
        }
    }

    // Update elapsed time timer
    if (ui_lbElapsedTime != NULL) {
        uint32_t total_sec = g_elapsed_time_sec;
        uint32_t hours = total_sec / 3600;
        uint32_t minutes = (total_sec % 3600) / 60;
        uint32_t seconds = total_sec % 60;

        char time_buf[16];  // "HH:MM:SS" = 8 chars + null
        (void)snprintf(time_buf, sizeof(time_buf), "%02lu.%02lu.%02lu",
                 (unsigned long)hours,
                 (unsigned long)minutes,
                 (unsigned long)seconds);
        lv_label_set_text(ui_lbElapsedTime, time_buf);
    }

    // Update setpoint indicators (green/yellow backgrounds)
    // This is called at 1Hz and reacts to:
    // - Mode changes (NORMAL ↔ SELECTING)
    // - Active setpoint changes (user or automatic)
    // - Timeout events
    update_setpoint_indicators();

    // ========================================================================
    // BATTERY LABEL UPDATE
    // ========================================================================
    if (ui_lbBat != NULL) {
        char bat_buf[24];  // worst-case snprintf: INT_MIN(11)+".X"(2)+"v "(2)+"100%"(4)+null
        (void)snprintf(bat_buf, sizeof(bat_buf), "%d.%dv %u%%",(int)(sensor_data.battery_voltage_mv / 1000), (int)((sensor_data.battery_voltage_mv /100) % 10), (unsigned int)sensor_data.battery_percentage);
        lv_label_set_text(ui_lbBat, bat_buf);

        // Color coding: Red if low, yellow if <30%, white otherwise
        if (sensor_data.battery_low) {
            lv_obj_set_style_text_color(ui_lbBat, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);  // Red
        } else if (sensor_data.battery_percentage < 30) {
            lv_obj_set_style_text_color(ui_lbBat, lv_color_hex(0xFFFF00), LV_PART_MAIN | LV_STATE_DEFAULT);  // Yellow
        } else {
            lv_obj_set_style_text_color(ui_lbBat, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);  // White
        }
    }
}

/**
 * @brief Handle button events on main screen
 * @param evt Button event (BTN_M_PRESS or BTN_S_PRESS)
 */
void screen_main_on_action(btn_event_t evt) {
    if (evt == BTN_S_PRESS) {
        if (selection_state == SELECTION_NORMAL) {
            // Enter SELECTING mode - highlight alternative
            selection_state = SELECTION_SELECTING;
            selection_timer = 0;  // Reset timeout timer
            update_setpoint_indicators();  // Immediate visual update
        } else {
            // Cancel - exit SELECTING mode without changing active setpoint
            selection_state = SELECTION_NORMAL;
            selection_timer = 0;
            update_setpoint_indicators();  // Immediate visual update
        }
    } else if (evt == BTN_M_PRESS) {
        if (selection_state == SELECTION_SELECTING) {
            // Confirm - swap active setpoint AND exit to NORMAL
            active_setpoint = (active_setpoint == 1) ? 2 : 1;
            selection_state = SELECTION_NORMAL;
            selection_timer = 0;
            update_setpoint_indicators();  // Immediate visual update
        } else {
            // In NORMAL mode: Enter menu
            screen_manager_switch(SCREEN_MENU);
        }
    }
}
