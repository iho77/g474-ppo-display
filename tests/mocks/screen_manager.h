/**
 * @file screen_manager.h
 * @brief screen_manager stub for native x86 unit testing
 *
 * sensor.c calls screen_manager_startup_complete() when sensors stabilize.
 * We stub it to a no-op so sensor.c compiles without LVGL.
 */
#pragma once

static inline void screen_manager_startup_complete(void) {}
