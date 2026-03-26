/**
 * @file dive_log.h
 * @brief Dive profile logging to W25Q128 external SPI NOR flash.
 *
 * Flash zone layout:
 *   Sector 18–19  (0x012000–0x013FFF, 8 KB)   Metadata ping-pong (24 bytes each)
 *   Sector 20+    (0x014000–0xFFFFFF, ~16 MB)  Append-only 32-byte entry log
 *
 * A dive log record consists of one DIVE_START entry followed by zero or more
 * DIVE_SAMPLE entries. A new dive starts when depth crosses 1 m.  A dive ends
 * after DIVE_LOG_SURFACE_TIMEOUT_SEC consecutive seconds below 1 m.
 *
 * Sample entries are written when depth changes > 0.5 m OR any sensor PPO
 * changes > 20 mbar since the last sample.
 */

#ifndef DIVE_LOG_H
#define DIVE_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32g4xx_hal.h"

/* ── Address map ── */
#define DIVE_META_BANK_A        0x012000UL  /* Sector 18: metadata ping */
#define DIVE_META_BANK_B        0x013000UL  /* Sector 19: metadata pong */
#define DIVE_LOG_DATA_START     0x014000UL  /* Sector 20: first data sector */
#define DIVE_LOG_DATA_SIZE      (4076UL * 4096UL)  /* Sectors 20–4095, ~16 MB */

/* ── Entry layout (32 bytes) ── */
#define DIVE_LOG_ENTRY_SIZE         32U
#define DIVE_LOG_ENTRY_TYPE_START   0xD1U   /* Dive begin marker */
#define DIVE_LOG_ENTRY_TYPE_SAMPLE  0xDAU   /* Measurement sample */
#define DIVE_LOG_ENTRY_BLANK        0xFFU   /* Erased NOR flash */

/* ── Metadata layout (24 bytes) ── */
#define DIVE_LOG_META_MAGIC         0xD1C1064EUL
#define DIVE_LOG_META_ENTRY_SIZE    24U

/* ── Dive lifecycle thresholds ── */
#define DIVE_LOG_START_DEPTH_MM         1000    /* Dive starts when depth >= 1 m */
#define DIVE_LOG_SURFACE_TIMEOUT_SEC    180U    /* Dive ends after 3 min at surface */
#define DIVE_LOG_DEPTH_THRESHOLD_CM     50U     /* Sample trigger: 0.5 m depth change */
#define DIVE_LOG_PPO_THRESHOLD_MBAR     20U     /* Sample trigger: 20 mbar PPO change */

/**
 * @brief Callback signature for dive_log_iterate().
 *
 * @param log_id       Dive sequence number (1, 2, 3 …)
 * @param sample_count Number of DIVE_SAMPLE entries in this dive
 * @param max_depth_cm Maximum depth reached during the dive (cm)
 * @param duration_sec Elapsed dive timer at the last sample (seconds)
 * @param user_data    Caller-supplied context pointer
 */
typedef void (*dive_log_iter_cb)(
    uint32_t log_id,
    uint32_t sample_count,
    uint16_t max_depth_cm,
    uint16_t duration_sec,
    void    *user_data
);

/**
 * @brief Initialise dive log module.
 *
 * Reads metadata ping-pong banks to recover write pointer and last log_id,
 * then performs a short forward scan to find the exact write position.
 * Must be called after ext_flash_storage_init().
 *
 * @param hspi  SPI handle for W25Q128 (hspi1).
 * @return true on success; false if SPI unresponsive (non-fatal, logging disabled).
 */
bool dive_log_init(SPI_HandleTypeDef *hspi);

/**
 * @brief 1 Hz tick — call from the 1 Hz flag block in the main loop.
 *
 * Detects dive start/end, enforces the 3-minute surface timeout, and writes
 * entries when depth or PPO thresholds are crossed.
 *
 * @param elapsed_sec  Current value of g_elapsed_time_sec from main.c.
 */
void dive_log_tick_1hz(uint32_t elapsed_sec);

/**
 * @brief Return total number of dives ever recorded (log_id of most recent dive).
 */
uint32_t dive_log_get_count(void);

/**
 * @brief Iterate through stored dives, reporting a summary for each.
 *
 * Scans the log from oldest to newest.  The callback is invoked for each
 * dive.  Use max_dives to limit the number reported (0 = all up to internal cap).
 *
 * @note This function performs SPI reads proportional to log size.
 *       Call from UI context only, never from a timing-sensitive path.
 *
 * @param cb         Callback invoked per dive.
 * @param user_data  Passed through to the callback.
 * @param max_dives  Maximum dives to process (capped at 10 internally; 0 = cap).
 * @return Number of dive summaries reported via the callback.
 */
uint32_t dive_log_iterate(dive_log_iter_cb cb, void *user_data, uint32_t max_dives);

/**
 * @brief Callback signature for dive_log_read_samples().
 *
 * @param sample_idx  0-based index of this sample within the dive
 * @param depth_cm    Depth in centimetres
 * @param timer_sec   Seconds elapsed since dive start
 * @param ppo0-2      PPO2 readings in mbar
 * @param bat_pct     Battery percentage
 * @param temp_c      Temperature in whole degrees Celsius
 * @param user_data   Caller-supplied context pointer
 */
typedef void (*dive_log_sample_cb)(
    uint32_t sample_idx,
    uint16_t depth_cm,
    uint16_t timer_sec,
    uint16_t ppo0, uint16_t ppo1, uint16_t ppo2,
    uint8_t  bat_pct,
    int16_t  temp_c,
    void    *user_data);

/**
 * @brief Read individual samples for a specific dive.
 *
 * Scans the log for entries belonging to log_id and invokes the callback
 * for samples in the range [skip, skip+max_count).  Useful for paginated
 * detail display.
 *
 * @note Performs SPI reads proportional to log size.  Call from UI context only.
 *
 * @param log_id     Dive sequence number to read samples from.
 * @param skip       Number of samples to skip (for pagination).
 * @param max_count  Maximum number of samples to report (0 = all remaining).
 * @param cb         Callback invoked per sample.
 * @param user_data  Passed through to the callback.
 * @return Number of samples reported via the callback.
 */
uint32_t dive_log_read_samples(uint32_t log_id, uint32_t skip, uint32_t max_count,
                                dive_log_sample_cb cb, void *user_data);

/**
 * @brief Factory reset — erase all dive log sectors (metadata + data).
 *
 * @note This operation erases all sectors and takes several seconds.
 *       Only call on explicit user request.
 * @return true if all sectors erased successfully; false if any sector failed
 *         (state not reset on failure — caller should report error and retry).
 */
bool dive_log_erase(void);

#endif /* DIVE_LOG_H */
