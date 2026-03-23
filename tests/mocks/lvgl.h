/**
 * @file lvgl.h
 * @brief Minimal LVGL stub for native x86 unit testing
 *
 * warning.h includes lvgl.h for lv_obj_t and related types used by
 * warning_apply_style(). That function is NOT under test (it needs a real
 * display), so these stubs are sufficient to satisfy the compiler.
 */
#pragma once
#include <stdint.h>

typedef struct lv_obj_t lv_obj_t;
struct lv_obj_t { uint32_t dummy; };

typedef uint32_t lv_color_t;
typedef uint8_t  lv_opa_t;

#define LV_OPA_TRANSP  0U
#define LV_OPA_COVER   255U
#define LV_PART_MAIN   0U
#define LV_STATE_DEFAULT 0U

static inline lv_color_t lv_color_hex(uint32_t c) { return (lv_color_t)c; }
static inline void lv_obj_set_style_border_color(lv_obj_t *o, lv_color_t c, uint32_t s) {
    (void)o; (void)c; (void)s;
}
static inline void lv_obj_set_style_bg_color(lv_obj_t *o, lv_color_t c, uint32_t s) {
    (void)o; (void)c; (void)s;
}
static inline void lv_obj_set_style_bg_opa(lv_obj_t *o, lv_opa_t opa, uint32_t s) {
    (void)o; (void)opa; (void)s;
}
static inline void lv_obj_set_style_text_color(lv_obj_t *o, lv_color_t c, uint32_t s) {
    (void)o; (void)c; (void)s;
}
