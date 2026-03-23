/**
 * @file stubs.c
 * @brief Linker-level stubs for HAL-dependent functions not under test.
 *
 * Production modules (settings_storage.c, sensor.c) call into hardware-
 * dependent code that we cannot link in for native x86 testing. These
 * non-inline stubs satisfy the linker without pulling in SPI/LVGL code.
 */

#include <stdint.h>
#include <stdbool.h>

/* ── Forward declarations for types used in stubs ───────────────────────── */
typedef struct settings_data_s settings_data_t;  /* opaque ptr sufficient */

/* ── ext_flash_storage.c stubs ──────────────────────────────────────────── */

bool ext_settings_save(const settings_data_t *s) { (void)s; return true; }
bool ext_settings_load(settings_data_t *s)       { (void)s; return false; }
void ext_settings_erase(void)                    {}

/* ── screen_manager.c stubs ─────────────────────────────────────────────── */

void screen_manager_startup_complete(void) {}
