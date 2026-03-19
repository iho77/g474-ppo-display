/**
 * @file screen_calibration.c
 * @brief Calibration screen - Sensor calibration workflow
 *
 * Guides user through two-point calibration process for O2 sensors.
 * BTN_M returns to menu, BTN_S advances calibration steps.
 */

#include "screen_manager.h"
#include "lvgl.h"
#include "sensor.h"
#include "calibration.h"
#include "flash_storage.h"
#include <stdio.h>  // For snprintf()

LV_FONT_DECLARE(lv_font_montserrat_28_new);

// ============================================================================
// PRIVATE VARIABLES - Widget references for update() callback
// ============================================================================

// Calibration screen state machine
static uint8_t cal_state = 0;           // Current state (0-5)
static uint16_t first_point_value = 21; // 1st calibration point (PPO2 reference, matches UI default)
static uint16_t second_point_value = 40; // 2nd calibration point (PPO2 reference, matches UI default)

// Widget array for state-based highlighting
static lv_obj_t* state_widgets[6] = {NULL};


lv_obj_t * ui_lblS1Sts = NULL;
lv_obj_t * ui_lblS2Sts = NULL;
lv_obj_t * ui_lblS3Sts = NULL;
lv_obj_t * ui_lblSnV2 = NULL;
lv_obj_t * ui_lblSnV1 = NULL;
lv_obj_t * ui_lblSnV3 = NULL;
lv_obj_t * ui_lblStatusBar1 = NULL;
lv_obj_t * ui_lblSV3 = NULL;
lv_obj_t * ui_lblStatusBar2 = NULL;
lv_obj_t * ui_lblMenuReset = NULL;
lv_obj_t * ui_lblMenuBack = NULL;
lv_obj_t * ui_lbl1stPoint = NULL;
lv_obj_t * ui_lbl1stPoint1 = NULL;
lv_obj_t * ui_lbl2ndPoint = NULL;
lv_obj_t * ui_lbl1stPoint3 = NULL;
lv_obj_t * ui_lblMnuCal1pt = NULL;
lv_obj_t * ui_lblMenuCal2pt = NULL;
lv_obj_t * ui_lblS1Cal2 = NULL;
lv_obj_t * ui_lblS1Cal3 = NULL;
lv_obj_t * ui_lblS1Cal4 = NULL;
lv_obj_t * ui_lblFeedback = NULL;

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr) {
    // Title

ui_lblS1Sts = lv_label_create(scr);
   lv_obj_set_width(ui_lblS1Sts, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS1Sts, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS1Sts, -104);
   lv_obj_set_y(ui_lblS1Sts, 33);
   lv_obj_set_align(ui_lblS1Sts, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS1Sts, "1pt");
   lv_obj_set_style_text_color(ui_lblS1Sts, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS1Sts, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS1Sts, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblS2Sts = lv_label_create(scr);
   lv_obj_set_width(ui_lblS2Sts, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS2Sts, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS2Sts, -5);
   lv_obj_set_y(ui_lblS2Sts, 33);
   lv_obj_set_align(ui_lblS2Sts, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS2Sts, "1pt");
   lv_obj_set_style_text_color(ui_lblS2Sts, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS2Sts, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS2Sts, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_bg_color(ui_lblS2Sts, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_bg_opa(ui_lblS2Sts, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblS3Sts = lv_label_create(scr);
   lv_obj_set_width(ui_lblS3Sts, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS3Sts, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS3Sts, 89);
   lv_obj_set_y(ui_lblS3Sts, 34);
   lv_obj_set_align(ui_lblS3Sts, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS3Sts, "n/a");
   lv_obj_set_style_text_color(ui_lblS3Sts, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS3Sts, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS3Sts, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_bg_color(ui_lblS3Sts, lv_color_hex(0xF90625), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_bg_opa(ui_lblS3Sts, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblSnV2 = lv_label_create(scr);
   lv_obj_set_width(ui_lblSnV2, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblSnV2, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblSnV2, -14);
   lv_obj_set_y(ui_lblSnV2, 57);
   lv_obj_set_align(ui_lblSnV2, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblSnV2, "10.1");
   lv_obj_set_style_text_color(ui_lblSnV2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblSnV2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblSnV2, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblSnV1 = lv_label_create(scr);
   lv_obj_set_width(ui_lblSnV1, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblSnV1, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblSnV1, -109);
   lv_obj_set_y(ui_lblSnV1, 56);
   lv_obj_set_align(ui_lblSnV1, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblSnV1, "10.1");
   lv_obj_set_style_text_color(ui_lblSnV1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblSnV1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblSnV1, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblSnV3 = lv_label_create(scr);
   lv_obj_set_width(ui_lblSnV3, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblSnV3, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblSnV3, 68);
   lv_obj_set_y(ui_lblSnV3, 57);
   lv_obj_set_align(ui_lblSnV3, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblSnV3, "0");
   lv_obj_set_style_text_color(ui_lblSnV3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblSnV3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblSnV3, &lv_font_montserrat_28_new, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblStatusBar1 = lv_label_create(scr);
   lv_obj_set_width(ui_lblStatusBar1, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblStatusBar1, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblStatusBar1, -108);
   lv_obj_set_y(ui_lblStatusBar1, -10);
   lv_obj_set_align(ui_lblStatusBar1, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblStatusBar1, "1st point:");
   lv_obj_set_scroll_dir(ui_lblStatusBar1, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblStatusBar1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblStatusBar1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblStatusBar1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblSV3 = lv_label_create(scr);
   lv_obj_set_width(ui_lblSV3, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblSV3, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblSV3, -407);
   lv_obj_set_y(ui_lblSV3, 265);
   lv_obj_set_align(ui_lblSV3, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblSV3, "10.1v");
   lv_obj_set_style_text_color(ui_lblSV3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblSV3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblSV3, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblStatusBar2 = lv_label_create(scr);
   lv_obj_set_width(ui_lblStatusBar2, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblStatusBar2, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblStatusBar2, -105);
   lv_obj_set_y(ui_lblStatusBar2, 17);
   lv_obj_set_align(ui_lblStatusBar2, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblStatusBar2, "2nd point");
   lv_obj_set_scroll_dir(ui_lblStatusBar2, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblStatusBar2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblStatusBar2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblStatusBar2, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblMenuReset = lv_label_create(scr);
   lv_obj_set_width(ui_lblMenuReset, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblMenuReset, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblMenuReset, -79);
   lv_obj_set_y(ui_lblMenuReset, 42);
   lv_obj_set_align(ui_lblMenuReset, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblMenuReset, "Reset calibration");
   lv_obj_set_scroll_dir(ui_lblMenuReset, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblMenuReset, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblMenuReset, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblMenuReset, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblMenuBack = lv_label_create(scr);
   lv_obj_set_width(ui_lblMenuBack, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblMenuBack, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblMenuBack, -119);
   lv_obj_set_y(ui_lblMenuBack, 76);
   lv_obj_set_align(ui_lblMenuBack, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblMenuBack, "Back");
   lv_obj_set_scroll_dir(ui_lblMenuBack, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblMenuBack, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblMenuBack, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblMenuBack, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lbl1stPoint = lv_label_create(scr);
   lv_obj_set_width(ui_lbl1stPoint, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lbl1stPoint, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lbl1stPoint, -27);
   lv_obj_set_y(ui_lbl1stPoint, -10);
   lv_obj_set_align(ui_lbl1stPoint, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lbl1stPoint, "21");
   lv_obj_clear_flag(ui_lbl1stPoint, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_MOMENTUM |
                     LV_OBJ_FLAG_SCROLL_CHAIN);     /// Flags
   lv_obj_set_scroll_dir(ui_lbl1stPoint, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lbl1stPoint, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lbl1stPoint, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lbl1stPoint, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lbl1stPoint1 = lv_label_create(scr);
   lv_obj_set_width(ui_lbl1stPoint1, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lbl1stPoint1, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lbl1stPoint1, 12);
   lv_obj_set_y(ui_lbl1stPoint1, -9);
   lv_obj_set_align(ui_lbl1stPoint1, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lbl1stPoint1, "%");
   lv_obj_set_scroll_dir(ui_lbl1stPoint1, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lbl1stPoint1, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lbl1stPoint1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lbl1stPoint1, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lbl2ndPoint = lv_label_create(scr);
   lv_obj_set_width(ui_lbl2ndPoint, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lbl2ndPoint, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lbl2ndPoint, -26);
   lv_obj_set_y(ui_lbl2ndPoint, 15);
   lv_obj_set_align(ui_lbl2ndPoint, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lbl2ndPoint, "40");
   lv_obj_set_scroll_dir(ui_lbl2ndPoint, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lbl2ndPoint, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lbl2ndPoint, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lbl2ndPoint, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lbl1stPoint3 = lv_label_create(scr);
   lv_obj_set_width(ui_lbl1stPoint3, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lbl1stPoint3, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lbl1stPoint3, 13);
   lv_obj_set_y(ui_lbl1stPoint3, 14);
   lv_obj_set_align(ui_lbl1stPoint3, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lbl1stPoint3, "%");
   lv_obj_set_scroll_dir(ui_lbl1stPoint3, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lbl1stPoint3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lbl1stPoint3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lbl1stPoint3, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblMnuCal1pt = lv_label_create(scr);
   lv_obj_set_width(ui_lblMnuCal1pt, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblMnuCal1pt, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblMnuCal1pt, 79);
   lv_obj_set_y(ui_lblMnuCal1pt, -11);
   lv_obj_set_align(ui_lblMnuCal1pt, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblMnuCal1pt, "Calibrate");
   lv_obj_set_scroll_dir(ui_lblMnuCal1pt, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblMnuCal1pt, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblMnuCal1pt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblMnuCal1pt, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblMenuCal2pt = lv_label_create(scr);
   lv_obj_set_width(ui_lblMenuCal2pt, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblMenuCal2pt, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblMenuCal2pt, 79);
   lv_obj_set_y(ui_lblMenuCal2pt, 16);
   lv_obj_set_align(ui_lblMenuCal2pt, LV_ALIGN_CENTER);
   lv_label_set_text(ui_lblMenuCal2pt, "Calibrate");
   lv_obj_set_scroll_dir(ui_lblMenuCal2pt, LV_DIR_LEFT);
   lv_obj_set_style_text_color(ui_lblMenuCal2pt, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblMenuCal2pt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblMenuCal2pt, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblS1Cal2 = lv_label_create(scr);
   lv_obj_set_width(ui_lblS1Cal2, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS1Cal2, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS1Cal2, -106);
   lv_obj_set_y(ui_lblS1Cal2, 10);
   lv_obj_set_align(ui_lblS1Cal2, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS1Cal2, "Sensor 1");
   lv_obj_set_style_text_color(ui_lblS1Cal2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS1Cal2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS1Cal2, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblS1Cal3 = lv_label_create(scr);
   lv_obj_set_width(ui_lblS1Cal3, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS1Cal3, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS1Cal3, -8);
   lv_obj_set_y(ui_lblS1Cal3, 10);
   lv_obj_set_align(ui_lblS1Cal3, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS1Cal3, "Sensor 2");
   lv_obj_set_style_text_color(ui_lblS1Cal3, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS1Cal3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS1Cal3, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   ui_lblS1Cal4 = lv_label_create(scr);
   lv_obj_set_width(ui_lblS1Cal4, LV_SIZE_CONTENT);   /// 1
   lv_obj_set_height(ui_lblS1Cal4, LV_SIZE_CONTENT);    /// 1
   lv_obj_set_x(ui_lblS1Cal4, 86);
   lv_obj_set_y(ui_lblS1Cal4, 11);
   lv_obj_set_align(ui_lblS1Cal4, LV_ALIGN_TOP_MID);
   lv_label_set_text(ui_lblS1Cal4, "Sensor 3");
   lv_obj_set_style_text_color(ui_lblS1Cal4, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblS1Cal4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblS1Cal4, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

   // Feedback message label at bottom of screen
   ui_lblFeedback = lv_label_create(scr);
   lv_obj_set_width(ui_lblFeedback, LV_SIZE_CONTENT);
   lv_obj_set_height(ui_lblFeedback, LV_SIZE_CONTENT);
   lv_obj_set_x(ui_lblFeedback, 0);
   lv_obj_set_y(ui_lblFeedback, -10);
   lv_obj_set_align(ui_lblFeedback, LV_ALIGN_BOTTOM_MID);
   lv_label_set_text(ui_lblFeedback, "");  // Empty by default
   lv_obj_set_style_text_color(ui_lblFeedback, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_opa(ui_lblFeedback, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
   lv_obj_set_style_text_font(ui_lblFeedback, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

}
// ============================================================================
// UI BUILDER IMPORT ZONE - END
// ============================================================================

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Set feedback message with color coding
 * @param msg Message text to display
 * @param success True for green (success), false for red (error)
 */
static void set_feedback(const char* msg, bool success) {
    if (ui_lblFeedback) {
        lv_label_set_text(ui_lblFeedback, msg);
        lv_color_t color = success ? lv_color_make(0, 255, 0) : lv_color_make(255, 0, 0);
        lv_obj_set_style_text_color(ui_lblFeedback, color, 0);
    }
}

/**
 * @brief Clear feedback message
 */
static void clear_feedback(void) {
    if (ui_lblFeedback) {
        lv_label_set_text(ui_lblFeedback, "");
    }
}

/**
 * @brief Update highlights based on current state
 */
static void update_highlights(void) {
    // Define highlight colors
    lv_color_t cyan = lv_color_make(0, 255, 255);
    lv_color_t white = lv_color_white();

    // Clear all highlights
    for (uint8_t i = 0; i < 6; i++) {
        if (state_widgets[i]) {
            lv_obj_set_style_text_color(state_widgets[i], white, 0);
        }
    }

    // Highlight current state
    if (state_widgets[cal_state]) {
        lv_obj_set_style_text_color(state_widgets[cal_state], cyan, 0);
    }

    // Clear feedback when state changes
    clear_feedback();
}

/**
 * @brief Advance to next state and update highlights
 */
static void advance_state(void) {
    cal_state = (cal_state + 1) % 6;  // Wrap around 0-5
    update_highlights();
}

/**
 * @brief Execute 1st point calibration for all sensors
 */
static void execute_first_point_calibration(void) {
    // Shared point 1: forced origin (0, 0)
    calibration_point p1;
    p1.raw = 0;
    p1.ppo = 0;
    sensor_data.s_cpt[0] = p1;

    // Shared point 2: use sensor 0 as reference for display
    calibration_point p2_display;
    p2_display.raw = (uint16_t)sensor_data.o2_s_uv[0];
    p2_display.ppo = first_point_value * 10;
    sensor_data.s_cpt[1] = p2_display;

    // Calculate coefficients for each sensor using their own current readings
    for (int sensor_idx = 0; sensor_idx < (SENSOR_COUNT - 2); sensor_idx++) {
        calibration_point p2;
        p2.raw = (uint16_t)sensor_data.o2_s_uv[sensor_idx];
        p2.ppo = first_point_value * 10;

        float a, b;
        a = (float)(p2.ppo - p1.ppo) / (float)(p2.raw - p1.raw);
        b = (float)(p2.raw * p1.ppo - p1.raw * p2.ppo) / (float)(p2.raw - p1.raw);

        sensor_data.s_cal[0][sensor_idx] = a;
        sensor_data.s_cal[1][sensor_idx] = b;
        sensor_data.os_s_cal_state[sensor_idx] = CALIBRATED_1PT;
    }

    // Save to flash and show feedback
    bool save_ok = flash_calibration_save();
    if (save_ok) {
        set_feedback("1st point calibrated", true);
    } else {
        set_feedback("Flash save failed!", false);
    }
}

/**
 * @brief Execute 2nd point calibration for all sensors
 */
static void execute_second_point_calibration(void) {
    // Update shared point 2 with sensor 0's reading for display
    calibration_point p2_display;
    p2_display.raw = (uint16_t)sensor_data.o2_s_uv[0];
    p2_display.ppo = second_point_value * 10;
    sensor_data.s_cpt[1] = p2_display;

    // Get shared point 1 (forced origin)
    calibration_point p1 = sensor_data.s_cpt[0];

    // Calculate coefficients for each sensor using their own current readings
    for (int sensor_idx = 0; sensor_idx < (SENSOR_COUNT - 2); sensor_idx++) {
        // Use this sensor's current reading
        calibration_point p2;
        p2.raw = (uint16_t)sensor_data.o2_s_uv[sensor_idx];
        p2.ppo = second_point_value * 10;

        // Calculate coefficients
        float a, b;
        a = (float)(p2.ppo - p1.ppo) / (float)(p2.raw - p1.raw);
        b = (float)(p2.raw * p1.ppo - p1.raw * p2.ppo) / (float)(p2.raw - p1.raw);

        // Store calibration coefficients
        sensor_data.s_cal[0][sensor_idx] = a;  // Slope
        sensor_data.s_cal[1][sensor_idx] = b;  // Y-intercept

        // Update calibration state (now has 2 points)
        sensor_data.os_s_cal_state[sensor_idx] = CALIBRATED_2PT;
    }

    // Save to flash and show feedback
    bool save_ok = flash_calibration_save();
    if (save_ok) {
        set_feedback("2nd point calibrated", true);
    } else {
        set_feedback("Flash save failed!", false);
    }
}

/**
 * @brief Reset calibration to factory defaults
 */
static void reset_calibration(void) {
    // Reset sensor state
    for (int sensor_idx = 0; sensor_idx < (SENSOR_COUNT - 2); sensor_idx++) {
        sensor_data.os_s_cal_state[sensor_idx] = NON_CALIBRATED;
        sensor_data.s_cal[0][sensor_idx] = 0;
        sensor_data.s_cal[1][sensor_idx] = 0;
    }

    // Erase flash page (factory reset)
    flash_calibration_erase();

    // Show feedback
    set_feedback("Calibration reset", true);
}

// ============================================================================
// SCREEN MANAGER INTERFACE
// ============================================================================

/**
 * @brief Factory function - creates and returns the calibration screen
 */
lv_obj_t* screen_calibration_create(void) {
    // Create root screen object
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK |
                        LV_OBJ_FLAG_SCROLLABLE);      /// Flags
      lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(scr, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    build_screen_content(scr);

    // Initialize state machine - reset to defaults each time screen is created
    cal_state = 0;
    first_point_value = 21;  // Reset to default
    second_point_value = 40; // Reset to default

    // Map widgets to state array
    state_widgets[0] = ui_lbl1stPoint;     // State 0: Edit 1st point value
    state_widgets[1] = ui_lblMnuCal1pt;    // State 1: Calibrate 1st point menu
    state_widgets[2] = ui_lbl2ndPoint;     // State 2: Edit 2nd point value
    state_widgets[3] = ui_lblMenuCal2pt;   // State 3: Calibrate 2nd point menu
    state_widgets[4] = ui_lblMenuReset;    // State 4: Reset menu
    state_widgets[5] = ui_lblMenuBack;     // State 5: Back menu

    // Ensure labels show default values (in case they were changed in previous session)
    lv_label_set_text(ui_lbl1stPoint, "21");
    lv_label_set_text(ui_lbl2ndPoint, "40");

    // Initial highlight (always starts at state 0)
    update_highlights();

    return scr;
}

/**
 * @brief Periodic update - called at 1Hz while this screen is active
 */
void screen_calibration_update(void) {
    char buf[16];

    // Update sensor voltage displays (in microvolts, format: XX.XX)
    if (ui_lblSnV1) {
        (void)snprintf(buf, sizeof(buf), "%d.%02d",
                 (int)(sensor_data.o2_s_uv[0] / 100),
                 (int)(sensor_data.o2_s_uv[0] % 100));
        lv_label_set_text(ui_lblSnV1, buf);
    }

    if (ui_lblSnV2) {
        (void)snprintf(buf, sizeof(buf), "%d.%02d",
                 (int)(sensor_data.o2_s_uv[1] / 100),
                 (int)(sensor_data.o2_s_uv[1] % 100));
        lv_label_set_text(ui_lblSnV2, buf);
    }

    if (ui_lblSnV3) {
        (void)snprintf(buf, sizeof(buf), "%d.%02d",
                 (int)(sensor_data.o2_s_uv[2] / 100),
                 (int)(sensor_data.o2_s_uv[2] % 100));
        lv_label_set_text(ui_lblSnV3, buf);
    }

    // Update calibration status labels with colored backgrounds
    // Map enum values to display strings
    const char* status_text[4] = {CAL_AUTO, CAL_1pt, CAL_2pt, FAULT};

    // Update sensor 1 status
    if (ui_lblS1Sts) {
        // Check sensor validity first
        if (!sensor_data.s_valid[0]) {
            // Invalid sensor - always show "n/a" on red background
            lv_label_set_text(ui_lblS1Sts, FAULT);
            lv_obj_set_style_bg_color(ui_lblS1Sts, lv_color_make(255, 0, 0), 0);
            lv_obj_set_style_bg_opa(ui_lblS1Sts, LV_OPA_COVER, 0);
        } else {
            // Valid sensor - show calibration status
            sensor_cal_state status = sensor_data.os_s_cal_state[0];
            if (status <= NON_CALIBRATED) {
                lv_label_set_text(ui_lblS1Sts, status_text[status]);

                // Set background color based on calibration state
                lv_color_t bg_color;
                if (status == AUTO_CALIBRATED) {
                    bg_color = lv_color_black();  // Auto: black
                } else if (status == CALIBRATED_1PT || status == CALIBRATED_2PT) {
                    bg_color = lv_color_make(0, 128, 0);  // 1pt/2pt: green
                } else {  // NON_CALIBRATED
                    bg_color = lv_color_make(255, 0, 0);  // n/a: red
                }
                lv_obj_set_style_bg_color(ui_lblS1Sts, bg_color, 0);
                lv_obj_set_style_bg_opa(ui_lblS1Sts, LV_OPA_COVER, 0);
            }
        }
    }

    // Update sensor 2 status
    if (ui_lblS2Sts) {
        // Check sensor validity first
        if (!sensor_data.s_valid[1]) {
            // Invalid sensor - always show "n/a" on red background
            lv_label_set_text(ui_lblS2Sts, "n/a");
            lv_obj_set_style_bg_color(ui_lblS2Sts, lv_color_make(255, 0, 0), 0);
            lv_obj_set_style_bg_opa(ui_lblS2Sts, LV_OPA_COVER, 0);
        } else {
            // Valid sensor - show calibration status
            sensor_cal_state status = sensor_data.os_s_cal_state[1];
            if (status <= NON_CALIBRATED) {
                lv_label_set_text(ui_lblS2Sts, status_text[status]);

                // Set background color based on calibration state
                lv_color_t bg_color;
                if (status == AUTO_CALIBRATED) {
                    bg_color = lv_color_black();  // Auto: black
                } else if (status == CALIBRATED_1PT || status == CALIBRATED_2PT) {
                    bg_color = lv_color_make(0, 128, 0);  // 1pt/2pt: green
                } else {  // NON_CALIBRATED
                    bg_color = lv_color_make(255, 0, 0);  // n/a: red
                }
                lv_obj_set_style_bg_color(ui_lblS2Sts, bg_color, 0);
                lv_obj_set_style_bg_opa(ui_lblS2Sts, LV_OPA_COVER, 0);
            }
        }
    }

    // Update sensor 3 status
    if (ui_lblS3Sts) {
        // Check sensor validity first
        if (!sensor_data.s_valid[2]) {
            // Invalid sensor - always show "n/a" on red background
            lv_label_set_text(ui_lblS3Sts, "n/a");
            lv_obj_set_style_bg_color(ui_lblS3Sts, lv_color_make(255, 0, 0), 0);
            lv_obj_set_style_bg_opa(ui_lblS3Sts, LV_OPA_COVER, 0);
        } else {
            // Valid sensor - show calibration status
            sensor_cal_state status = sensor_data.os_s_cal_state[2];
            if (status <= NON_CALIBRATED) {
                lv_label_set_text(ui_lblS3Sts, status_text[status]);

                // Set background color based on calibration state
                lv_color_t bg_color;
                if (status == AUTO_CALIBRATED) {
                    bg_color = lv_color_black();  // Auto: black
                } else if (status == CALIBRATED_1PT || status == CALIBRATED_2PT) {
                    bg_color = lv_color_make(0, 128, 0);  // 1pt/2pt: green
                } else {  // NON_CALIBRATED
                    bg_color = lv_color_make(255, 0, 0);  // n/a: red
                }
                lv_obj_set_style_bg_color(ui_lblS3Sts, bg_color, 0);
                lv_obj_set_style_bg_opa(ui_lblS3Sts, LV_OPA_COVER, 0);
            }
        }
    }
}

/**
 * @brief Handle button events for calibration screen
 * @param evt Button event (BTN_M_PRESS or BTN_S_PRESS)
 */
void screen_calibration_on_button(btn_event_t evt) {
    char buf[8];

    switch (cal_state) {
        case 0:  // State 0: lbl1stPoint editing
            if (evt == BTN_M_PRESS) {
                // Advance to state 1
                advance_state();
            } else if (evt == BTN_S_PRESS) {
                // Cycle 1st point value: 21-40
                first_point_value++;
                if (first_point_value > 40) {
                    first_point_value = 21;
                }
                (void)snprintf(buf, sizeof(buf), "%u", first_point_value);
                lv_label_set_text(ui_lbl1stPoint, buf);
            }
            break;

        case 1:  // State 1: lblMnuCal1pt menu
            if (evt == BTN_M_PRESS) {
                // Advance to state 2
                advance_state();
            } else if (evt == BTN_S_PRESS) {
                // Execute 1st point calibration
                execute_first_point_calibration();
            }
            break;

        case 2:  // State 2: lbl2ndPoint editing
            if (evt == BTN_M_PRESS) {
                // Advance to state 3
                advance_state();
            } else if (evt == BTN_S_PRESS) {
                // Cycle 2nd point value: 41-100
                second_point_value++;
                if (second_point_value > 100) {
                    second_point_value = 41;
                }
                (void)snprintf(buf, sizeof(buf), "%u", second_point_value);
                lv_label_set_text(ui_lbl2ndPoint, buf);
            }
            break;

        case 3:  // State 3: lblMenuCal2pt menu
            if (evt == BTN_M_PRESS) {
                // Advance to state 4
                advance_state();
            } else if (evt == BTN_S_PRESS) {
                // Execute 2nd point calibration
                execute_second_point_calibration();
            }
            break;

        case 4:  // State 4: lblMenuReset menu
            if (evt == BTN_M_PRESS) {
                // Advance to state 5
                advance_state();
            } else if (evt == BTN_S_PRESS) {
                // Execute reset calibration (stub)
                reset_calibration();
            }
            break;

        case 5:  // State 5: lblMenuBack menu
            if (evt == BTN_M_PRESS) {
                // Wrap to state 0
                cal_state = 0;
                update_highlights();
            } else if (evt == BTN_S_PRESS) {
                // Switch to main screen
                screen_manager_switch(SCREEN_MAIN);
            }
            break;

        default:
            cal_state = 0;  // Safety fallback
            update_highlights();
            break;
    }
}
