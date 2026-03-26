/**
 * @file screen_dive_log.c
 * @brief Dive Log screen - Historical dive data viewer
 *
 * Two modes share one screen:
 *   LIST   - up to 10 most recent dives + Back entry.
 *             BTN_M cycles highlight, BTN_S selects.
 *   DETAIL - paginated sample view for a selected dive.
 *             BTN_M returns to list, BTN_S advances page.
 *
 * All labels use lv_font_montserrat_14.
 */

#include "screen_manager.h"
#include "dive_log.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// CONSTANTS
// ============================================================================
#define DL_MAX_DIVES    10
#define DL_DETAIL_ROWS  8

// ============================================================================
// PRIVATE TYPES
// ============================================================================
typedef enum { DL_VIEW_LIST, DL_VIEW_DETAIL } dl_view_t;

typedef struct {
    uint32_t log_id;
    uint32_t sample_count;
    uint16_t max_depth_cm;
    uint16_t duration_sec;
} dl_summary_t;

// Context passed into dive_log_read_samples callback
typedef struct {
    uint16_t depth_cm;
    uint16_t timer_sec;
    uint16_t ppo[3];
    uint8_t  bat_pct;
    int16_t  temp_c;
    bool     valid;
} dl_sample_t;

typedef struct {
    dl_sample_t *rows;
    int          capacity;
    int          count;
} dl_sample_ctx_t;

// ============================================================================
// PRIVATE STATE
// ============================================================================
static dl_view_t    dl_view;
static int          dl_selection;    /* highlighted row index (0..dl_row_count-1) */
static int          dl_row_count;    /* dl_list_count + 1 (Back entry) */
static int          dl_list_count;   /* dives loaded, <= DL_MAX_DIVES */
static int          dl_detail_page;  /* 0-based page in detail view */

static dl_summary_t dl_summaries[DL_MAX_DIVES];

// ============================================================================
// PRIVATE WIDGETS — list view
// ============================================================================
static lv_obj_t *lbl_title     = NULL;
static lv_obj_t *lbl_list[DL_MAX_DIVES + 1];   /* dive rows + Back */
static lv_obj_t *lbl_list_hint = NULL;

// ============================================================================
// PRIVATE WIDGETS — detail view (hidden when in list mode)
// ============================================================================
static lv_obj_t *lbl_det_title = NULL;
static lv_obj_t *lbl_det_rows[DL_DETAIL_ROWS];
static lv_obj_t *lbl_det_hint  = NULL;

// ============================================================================
// CALLBACKS FOR dive_log API
// ============================================================================

static int dl_iter_idx = 0;

static void dl_iter_cb(uint32_t log_id, uint32_t sample_count,
                       uint16_t max_depth_cm, uint16_t duration_sec,
                       void *user_data)
{
    (void)user_data;
    if (dl_iter_idx >= DL_MAX_DIVES) return;
    dl_summaries[dl_iter_idx].log_id       = log_id;
    dl_summaries[dl_iter_idx].sample_count = sample_count;
    dl_summaries[dl_iter_idx].max_depth_cm = max_depth_cm;
    dl_summaries[dl_iter_idx].duration_sec = duration_sec;
    dl_iter_idx++;
}

static void dl_sample_cb(uint32_t sample_idx,
                          uint16_t depth_cm, uint16_t timer_sec,
                          uint16_t ppo0, uint16_t ppo1, uint16_t ppo2,
                          uint8_t bat_pct, int16_t temp_c,
                          void *user_data)
{
    dl_sample_ctx_t *ctx = (dl_sample_ctx_t *)user_data;
    if (ctx == NULL || ctx->count >= ctx->capacity) return;
    int i = ctx->count;
    (void)sample_idx;
    ctx->rows[i].depth_cm  = depth_cm;
    ctx->rows[i].timer_sec = timer_sec;
    ctx->rows[i].ppo[0]    = ppo0;
    ctx->rows[i].ppo[1]    = ppo1;
    ctx->rows[i].ppo[2]    = ppo2;
    ctx->rows[i].bat_pct   = bat_pct;
    ctx->rows[i].temp_c    = temp_c;
    ctx->rows[i].valid     = true;
    ctx->count++;
}

// ============================================================================
// PRIVATE HELPERS
// ============================================================================

static void dl_refresh_list(void)
{
    static char buf[48];

    for (int i = 0; i < dl_list_count; i++) {
        if (lbl_list[i] == NULL) continue;
        uint32_t dep_m  = dl_summaries[i].max_depth_cm / 100U;
        uint32_t dep_dm = (dl_summaries[i].max_depth_cm % 100U) / 10U;
        uint32_t dur_m  = dl_summaries[i].duration_sec / 60U;
        uint32_t dur_s  = dl_summaries[i].duration_sec % 60U;
        (void)snprintf(buf, sizeof(buf), "#%-4lu  %lu.%lum  %lum%02lus",
                       (unsigned long)dl_summaries[i].log_id,
                       (unsigned long)dep_m, (unsigned long)dep_dm,
                       (unsigned long)dur_m, (unsigned long)dur_s);
        lv_label_set_text(lbl_list[i], buf);
    }

    /* Back entry */
    if (lbl_list[dl_list_count] != NULL) {
        lv_label_set_text(lbl_list[dl_list_count], "<- Back");
    }

    /* Colours */
    for (int i = 0; i < dl_row_count; i++) {
        if (lbl_list[i] == NULL) continue;
        if (i == dl_selection) {
            lv_obj_set_style_text_color(lbl_list[i], lv_color_make(0, 255, 255), 0);
        } else {
            lv_obj_set_style_text_color(lbl_list[i], lv_color_white(), 0);
        }
    }
}

static void dl_show_detail_page(void)
{
    static char title_buf[32];
    static char row_buf[64];
    static dl_sample_t sample_data[DL_DETAIL_ROWS];

    dl_sample_ctx_t ctx = {
        .rows     = sample_data,
        .capacity = DL_DETAIL_ROWS,
        .count    = 0
    };
    for (int i = 0; i < DL_DETAIL_ROWS; i++) {
        sample_data[i].valid = false;
    }

    uint32_t skip      = (uint32_t)dl_detail_page * (uint32_t)DL_DETAIL_ROWS;
    uint32_t log_id    = dl_summaries[dl_selection].log_id;
    uint32_t n         = dive_log_read_samples(log_id, skip, (uint32_t)DL_DETAIL_ROWS,
                                               dl_sample_cb, &ctx);

    /* If page is empty (past end), wrap to page 0 */
    if (n == 0U && dl_detail_page > 0) {
        dl_detail_page = 0;
        dl_show_detail_page();
        return;
    }

    (void)snprintf(title_buf, sizeof(title_buf), "Dive #%lu  pg %d",
                   (unsigned long)log_id, dl_detail_page + 1);
    if (lbl_det_title != NULL) {
        lv_label_set_text(lbl_det_title, title_buf);
    }

    for (int i = 0; i < DL_DETAIL_ROWS; i++) {
        if (lbl_det_rows[i] == NULL) continue;
        if (!sample_data[i].valid) {
            lv_label_set_text(lbl_det_rows[i], "");
            continue;
        }
        uint32_t t_min = sample_data[i].timer_sec / 60U;
        uint32_t t_sec = sample_data[i].timer_sec % 60U;
        uint32_t d_m   = sample_data[i].depth_cm / 100U;
        uint32_t d_dm  = (sample_data[i].depth_cm % 100U) / 10U;
        uint32_t p0i   = sample_data[i].ppo[0] / 1000U;
        uint32_t p0f   = (sample_data[i].ppo[0] % 1000U + 5U) / 10U;
        uint32_t p1i   = sample_data[i].ppo[1] / 1000U;
        uint32_t p1f   = (sample_data[i].ppo[1] % 1000U + 5U) / 10U;
        uint32_t p2i   = sample_data[i].ppo[2] / 1000U;
        uint32_t p2f   = (sample_data[i].ppo[2] % 1000U + 5U) / 10U;
        (void)snprintf(row_buf, sizeof(row_buf),
                       "T:%lum%02lus D:%lu.%lum P:%lu.%02lu/%lu.%02lu/%lu.%02lu",
                       (unsigned long)t_min, (unsigned long)t_sec,
                       (unsigned long)d_m,   (unsigned long)d_dm,
                       (unsigned long)p0i,   (unsigned long)p0f,
                       (unsigned long)p1i,   (unsigned long)p1f,
                       (unsigned long)p2i,   (unsigned long)p2f);
        lv_label_set_text(lbl_det_rows[i], row_buf);
    }
}

static void dl_switch_to_detail(void)
{
    /* Hide list labels */
    lv_obj_add_flag(lbl_title,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_list_hint, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < dl_row_count; i++) {
        if (lbl_list[i] != NULL) lv_obj_add_flag(lbl_list[i], LV_OBJ_FLAG_HIDDEN);
    }
    /* Show detail labels */
    lv_obj_clear_flag(lbl_det_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_det_hint,  LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < DL_DETAIL_ROWS; i++) {
        if (lbl_det_rows[i] != NULL) lv_obj_clear_flag(lbl_det_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    dl_show_detail_page();
}

static void dl_switch_to_list(void)
{
    /* Hide detail labels */
    lv_obj_add_flag(lbl_det_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_det_hint,  LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < DL_DETAIL_ROWS; i++) {
        if (lbl_det_rows[i] != NULL) lv_obj_add_flag(lbl_det_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    /* Show list labels */
    lv_obj_clear_flag(lbl_title,     LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_list_hint, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < dl_row_count; i++) {
        if (lbl_list[i] != NULL) lv_obj_clear_flag(lbl_list[i], LV_OBJ_FLAG_HIDDEN);
    }
    dl_refresh_list();
}

// ============================================================================
// UI BUILDER IMPORT ZONE - START
// ============================================================================
static void build_screen_content(lv_obj_t* scr)
{
    /* ---- List view: title ---- */
    lbl_title = lv_label_create(scr);
    lv_obj_set_pos(lbl_title, 10, 3);
    lv_label_set_text(lbl_title, "Dive Log");
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ---- List view: dive rows + Back ---- */
    for (int i = 0; i <= DL_MAX_DIVES; i++) {
        lbl_list[i] = lv_label_create(scr);
        lv_obj_set_pos(lbl_list[i], (lv_coord_t)10, (lv_coord_t)(22 + i * 18));
        lv_label_set_text(lbl_list[i], "");
        lv_obj_set_style_text_color(lbl_list[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl_list[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* ---- List view: hint ---- */
    lbl_list_hint = lv_label_create(scr);
    lv_obj_set_pos(lbl_list_hint, 10, 205);
    lv_label_set_text(lbl_list_hint, "M:next  S:open");
    lv_obj_set_style_text_color(lbl_list_hint, lv_color_make(150, 150, 150), 0);
    lv_obj_set_style_text_font(lbl_list_hint, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    /* ---- Detail view: title (hidden) ---- */
    lbl_det_title = lv_label_create(scr);
    lv_obj_set_pos(lbl_det_title, 10, 3);
    lv_label_set_text(lbl_det_title, "Dive #-");
    lv_obj_set_style_text_color(lbl_det_title, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl_det_title, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(lbl_det_title, LV_OBJ_FLAG_HIDDEN);

    /* ---- Detail view: sample rows (hidden) ---- */
    for (int i = 0; i < DL_DETAIL_ROWS; i++) {
        lbl_det_rows[i] = lv_label_create(scr);
        lv_obj_set_pos(lbl_det_rows[i], 10, 22 + i * 22);
        lv_label_set_text(lbl_det_rows[i], "");
        lv_obj_set_style_text_color(lbl_det_rows[i], lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl_det_rows[i], &lv_font_montserrat_14,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_flag(lbl_det_rows[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* ---- Detail view: hint (hidden) ---- */
    lbl_det_hint = lv_label_create(scr);
    lv_obj_set_pos(lbl_det_hint, 10, 205);
    lv_label_set_text(lbl_det_hint, "M:back  S:next pg");
    lv_obj_set_style_text_color(lbl_det_hint, lv_color_make(150, 150, 150), 0);
    lv_obj_set_style_text_font(lbl_det_hint, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(lbl_det_hint, LV_OBJ_FLAG_HIDDEN);
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
lv_obj_t* screen_dive_log_create(void)
{
    /* Reset state */
    dl_view        = DL_VIEW_LIST;
    dl_selection   = 0;
    dl_detail_page = 0;
    dl_list_count  = 0;
    dl_iter_idx    = 0;
    memset(dl_summaries, 0, sizeof(dl_summaries));

    /* Load dive summaries (newest-first, up to 10) */
    dive_log_iterate(dl_iter_cb, NULL, 0U);
    dl_list_count = dl_iter_idx;
    dl_row_count  = dl_list_count + 1;  /* +1 for Back entry */

    /* Create root screen */
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    build_screen_content(scr);

    /* Hide rows beyond what we have */
    for (int i = dl_row_count; i <= DL_MAX_DIVES; i++) {
        if (lbl_list[i] != NULL) lv_obj_add_flag(lbl_list[i], LV_OBJ_FLAG_HIDDEN);
    }

    /* Populate and highlight */
    dl_refresh_list();

    return scr;
}

/**
 * @brief Action handler - called for all button events on this screen
 */
void screen_dive_log_action(btn_event_t evt)
{
    if (dl_view == DL_VIEW_LIST) {
        if (evt == BTN_M_PRESS) {
            /* Cycle selection */
            if (lbl_list[dl_selection] != NULL) {
                lv_obj_set_style_text_color(lbl_list[dl_selection], lv_color_white(), 0);
            }
            dl_selection = (dl_selection + 1) % dl_row_count;
            if (lbl_list[dl_selection] != NULL) {
                lv_obj_set_style_text_color(lbl_list[dl_selection],
                                            lv_color_make(0, 255, 255), 0);
            }
        } else if (evt == BTN_S_PRESS) {
            if (dl_selection == dl_list_count) {
                /* Back entry selected */
                screen_manager_switch(SCREEN_MENU);
            } else if (dl_list_count > 0) {
                /* Enter detail view */
                dl_view        = DL_VIEW_DETAIL;
                dl_detail_page = 0;
                dl_switch_to_detail();
            }
        }
    } else {
        /* DL_VIEW_DETAIL */
        if (evt == BTN_M_PRESS) {
            dl_view = DL_VIEW_LIST;
            dl_switch_to_list();
        } else if (evt == BTN_S_PRESS) {
            dl_detail_page++;
            dl_show_detail_page();  /* wraps back to page 0 if empty */
        }
    }
}
