/**
 * @file dive_log.c
 * @brief Dive profile logging to W25Q128 external SPI NOR flash.
 *
 * Entry format (32 bytes):
 *   [0]      type          DIVE_LOG_ENTRY_TYPE_START or _SAMPLE
 *   [1]      flags         reserved, 0
 *   [2-3]    crc16         CRC16-CCITT over bytes [0..31] with [2-3]=0 during calc
 *   [4-7]    log_id        uint32_t, dive sequence number
 *   [8-9]    depth_cm      uint16_t, depth in cm
 *   [10-11]  timer_sec     uint16_t, seconds since dive start
 *   [12-13]  ppo[0]        uint16_t, mbar
 *   [14-15]  ppo[1]        uint16_t, mbar
 *   [16-17]  ppo[2]        uint16_t, mbar
 *   [18]     battery_pct   uint8_t
 *   [19]     reserved      0
 *   [20-21]  temperature_c int16_t, whole degrees Celsius
 *   [22-31]  reserved      0
 *
 * Metadata format (24 bytes, ping-pong in sectors 18-19):
 *   [0-3]    magic         DIVE_LOG_META_MAGIC
 *   [4-7]    sequence      uint32_t, increments on each save
 *   [8-11]   write_offset  uint32_t, data-zone-relative byte offset
 *   [12-15]  last_log_id   uint32_t
 *   [16-19]  entry_count   uint32_t
 *   [20-23]  crc32         CRC32 over bytes [0-19]
 */

#include "dive_log.h"
#include "w25q128.h"
#include "flash_utils.h"
#include "sensor.h"
#include <string.h>

extern volatile sensor_data_t sensor_data;

/* ── Module state ── */
typedef struct {
    SPI_HandleTypeDef *hspi;
    uint32_t  write_offset;          /* data-zone-relative byte offset          */
    uint32_t  log_id;                /* most recent (or current) dive ID        */
    uint32_t  entry_count;           /* total entries written (mirrored in meta) */
    uint32_t  meta_sequence;         /* sequence number for next metadata write  */
    uint8_t   active_meta_bank;      /* 0 = Bank A, 1 = Bank B                  */
    bool      dive_active;
    bool      initialized;
    uint8_t   pad;
    uint32_t  dive_start_timer;      /* elapsed_sec at dive start                */
    uint16_t  prev_depth_cm;
    uint16_t  prev_ppo[3];           /* mbar, last written sample per sensor     */
    uint32_t  pending_erase_addr;    /* sector address awaiting deferred erase   */
    uint16_t  surface_sec_counter;   /* seconds at < 1 m while dive_active       */
    uint8_t   pad2[2];
} dive_log_state_t;

static dive_log_state_t g_dl;

/* ============================================================================
 * HELPERS: little-endian byte packing (pattern shared with ext_flash_storage)
 * ========================================================================= */

static inline void put_u16(uint8_t *buf, uint16_t v)
{
    buf[0] = (uint8_t)(v & 0xFFU);
    buf[1] = (uint8_t)((v >> 8U) & 0xFFU);
}

static inline void put_u32(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v & 0xFFU);
    buf[1] = (uint8_t)((v >>  8U) & 0xFFU);
    buf[2] = (uint8_t)((v >> 16U) & 0xFFU);
    buf[3] = (uint8_t)((v >> 24U) & 0xFFU);
}

static inline uint16_t get_u16(const uint8_t *buf)
{
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8U);
}

static inline uint32_t get_u32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] <<  8U)
         | ((uint32_t)buf[2] << 16U)
         | ((uint32_t)buf[3] << 24U);
}

/* ============================================================================
 * ADDRESS HELPERS
 * ========================================================================= */

static inline uint32_t data_off_to_addr(uint32_t off)
{
    return DIVE_LOG_DATA_START + off;
}

static inline uint32_t meta_bank_addr(uint8_t bank)
{
    return (bank == 0U) ? DIVE_META_BANK_A : DIVE_META_BANK_B;
}

/* ============================================================================
 * METADATA: pack / read / save
 * ========================================================================= */

static void meta_pack(uint8_t buf[DIVE_LOG_META_ENTRY_SIZE])
{
    memset(buf, 0, DIVE_LOG_META_ENTRY_SIZE);
    put_u32(&buf[0],  DIVE_LOG_META_MAGIC);
    put_u32(&buf[4],  g_dl.meta_sequence);
    put_u32(&buf[8],  g_dl.write_offset);
    put_u32(&buf[12], g_dl.log_id);
    put_u32(&buf[16], g_dl.entry_count);
    /* bytes [20-23]: crc placeholder = 0 */
    uint32_t crc = flash_crc32(buf, 20U);
    put_u32(&buf[20], crc);
}

static bool meta_read_bank(uint8_t bank, uint32_t *seq_out,
                            uint32_t *write_off_out, uint32_t *log_id_out,
                            uint32_t *entry_count_out)
{
    uint8_t buf[DIVE_LOG_META_ENTRY_SIZE];
    if (w25q128_read(g_dl.hspi, meta_bank_addr(bank), buf,
                     DIVE_LOG_META_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }
    if (get_u32(&buf[0]) != DIVE_LOG_META_MAGIC) {
        return false;
    }
    uint8_t tmp[DIVE_LOG_META_ENTRY_SIZE];
    memcpy(tmp, buf, DIVE_LOG_META_ENTRY_SIZE);
    put_u32(&tmp[20], 0U);
    if (flash_crc32(tmp, 20U) != get_u32(&buf[20])) {
        return false;
    }
    *seq_out         = get_u32(&buf[4]);
    *write_off_out   = get_u32(&buf[8]);
    *log_id_out      = get_u32(&buf[12]);
    *entry_count_out = get_u32(&buf[16]);
    return true;
}

/* Write metadata to the inactive bank, then make it active. */
static void meta_save(void)
{
    uint8_t target = (g_dl.active_meta_bank == 0U) ? 1U : 0U;
    uint32_t addr  = meta_bank_addr(target);

    g_dl.meta_sequence++;
    if (g_dl.meta_sequence == 0U) g_dl.meta_sequence = 1U;

    uint8_t buf[DIVE_LOG_META_ENTRY_SIZE];
    meta_pack(buf);

    if (w25q128_erase_sector(g_dl.hspi, addr) != W25Q128_OK) {
        g_dl.meta_sequence--;
        return;
    }
    if (w25q128_write_page(g_dl.hspi, addr, buf, DIVE_LOG_META_ENTRY_SIZE) != W25Q128_OK) {
        g_dl.meta_sequence--;
        return;
    }
    g_dl.active_meta_bank = target;
}

/* ============================================================================
 * DATA ENTRIES: pack / write
 * ========================================================================= */

static void entry_pack(uint8_t buf[DIVE_LOG_ENTRY_SIZE],
                       uint8_t type, uint32_t log_id,
                       uint16_t depth_cm, uint16_t timer_sec,
                       const uint16_t ppo[3],
                       uint8_t bat_pct, int16_t temperature_c)
{
    memset(buf, 0, DIVE_LOG_ENTRY_SIZE);
    buf[0] = type;
    buf[1] = 0U;
    /* [2-3] crc placeholder = 0 */
    put_u32(&buf[4],  log_id);
    put_u16(&buf[8],  depth_cm);
    put_u16(&buf[10], timer_sec);
    put_u16(&buf[12], ppo[0]);
    put_u16(&buf[14], ppo[1]);
    put_u16(&buf[16], ppo[2]);
    buf[18] = bat_pct;
    buf[19] = 0U;
    put_u16(&buf[20], (uint16_t)temperature_c);
    /* [22-31] reserved = 0 */

    uint16_t crc = flash_crc16_ccitt(buf, DIVE_LOG_ENTRY_SIZE);
    put_u16(&buf[2], crc);
}

/* Write one entry at the current write_offset; advance pointer; handle wrap. */
static bool entry_write(uint8_t type, uint16_t depth_cm, uint16_t timer_sec,
                        const uint16_t ppo[3],
                        uint8_t bat_pct, int16_t temperature_c)
{
    if (g_dl.pending_erase_addr != 0U) {
        return false;   /* deferred erase not yet executed */
    }

    uint8_t buf[DIVE_LOG_ENTRY_SIZE];
    entry_pack(buf, type, g_dl.log_id, depth_cm, timer_sec,
               ppo, bat_pct, temperature_c);

    if (w25q128_write_page(g_dl.hspi, data_off_to_addr(g_dl.write_offset),
                           buf, DIVE_LOG_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }

    g_dl.write_offset += DIVE_LOG_ENTRY_SIZE;
    g_dl.entry_count++;

    /* Zone full — wrap and schedule erase of the oldest sector */
    if (g_dl.write_offset >= DIVE_LOG_DATA_SIZE) {
        g_dl.write_offset = 0U;
        g_dl.pending_erase_addr = DIVE_LOG_DATA_START;
    }

    return true;
}

/* ============================================================================
 * BOOT: forward scan to exact write pointer
 * ========================================================================= */

static void recover_write_offset(uint32_t metadata_offset)
{
    /* Bound the scan to metadata_offset + (entry_count + 1) entries so that
     * a corrupt non-blank, non-valid byte cannot force a full ~524K-iteration
     * scan (~52 s blocking at 100 µs/read).  The +1 covers one entry written
     * between the last meta_save and the power loss. */
    uint32_t max_scan_entries = g_dl.entry_count + 1U;
    uint32_t scan_end = metadata_offset + max_scan_entries * DIVE_LOG_ENTRY_SIZE;
    if (scan_end > DIVE_LOG_DATA_SIZE) scan_end = DIVE_LOG_DATA_SIZE;

    uint8_t type_byte;
    uint32_t off = metadata_offset;

    while (off < scan_end) {
        if (w25q128_read(g_dl.hspi, data_off_to_addr(off),
                         &type_byte, 1U) != W25Q128_OK) {
            break;
        }
        if (type_byte == DIVE_LOG_ENTRY_BLANK) {
            break;
        }
        off += DIVE_LOG_ENTRY_SIZE;
    }
    g_dl.write_offset = (off < DIVE_LOG_DATA_SIZE) ? off : 0U;
}

/* ============================================================================
 * PUBLIC: INIT
 * ========================================================================= */

bool dive_log_init(SPI_HandleTypeDef *hspi)
{
    memset(&g_dl, 0, sizeof(g_dl));
    g_dl.hspi = hspi;

    uint32_t seq_a = 0U, off_a = 0U, id_a = 0U, cnt_a = 0U;
    uint32_t seq_b = 0U, off_b = 0U, id_b = 0U, cnt_b = 0U;
    bool valid_a = meta_read_bank(0U, &seq_a, &off_a, &id_a, &cnt_a);
    bool valid_b = meta_read_bank(1U, &seq_b, &off_b, &id_b, &cnt_b);

    if (valid_a || valid_b) {
        bool use_a = valid_a && (!valid_b || (seq_a >= seq_b));
        if (use_a) {
            g_dl.write_offset     = off_a;
            g_dl.log_id           = id_a;
            g_dl.entry_count      = cnt_a;
            g_dl.meta_sequence    = seq_a;
            g_dl.active_meta_bank = 0U;
        } else {
            g_dl.write_offset     = off_b;
            g_dl.log_id           = id_b;
            g_dl.entry_count      = cnt_b;
            g_dl.meta_sequence    = seq_b;
            g_dl.active_meta_bank = 1U;
        }
        /* Advance past any entries written after the last metadata save */
        recover_write_offset(g_dl.write_offset);
    }
    /* else: fresh chip — all fields remain 0 from memset */

    g_dl.initialized = true;
    return true;
}

/* ============================================================================
 * PUBLIC: 1 Hz TICK
 * ========================================================================= */

void dive_log_tick_1hz(uint32_t elapsed_sec)
{
    if (!g_dl.initialized) return;

    /* Atomic snapshot of volatile sensor_data — prevents torn reads between
     * ADC ISR updates and the multi-field access below (critical section < 1 µs) */
    __disable_irq();
    bool     snap_p_valid  = sensor_data.pressure.pressure_valid;
    int32_t  snap_depth_mm = sensor_data.pressure.depth_mm;
    int32_t  snap_temp_raw = sensor_data.pressure.temperature_c;
    uint8_t  snap_bat_pct  = sensor_data.battery_percentage;
    int32_t  snap_ppo[3]   = { sensor_data.o2_s_ppo[0],
                                sensor_data.o2_s_ppo[1],
                                sensor_data.o2_s_ppo[2] };
    bool     snap_valid[3] = { sensor_data.s_valid[0],
                                sensor_data.s_valid[1],
                                sensor_data.s_valid[2] };
    __enable_irq();

    if (!snap_p_valid) return;

    int32_t  depth_mm     = snap_depth_mm;
    uint32_t depth_mm_u   = (depth_mm > 0) ? (uint32_t)depth_mm : 0U;
    uint16_t depth_cm     = (depth_mm_u > 655350U) ? 0xFFFFU : (uint16_t)(depth_mm_u / 10U);
    int16_t  temperature  = (int16_t)snap_temp_raw;
    uint8_t  bat_pct      = snap_bat_pct;

    /* ── Execute deferred sector erase when safely at surface ── */
    if (g_dl.pending_erase_addr != 0U && !g_dl.dive_active) {
        if (w25q128_erase_sector(g_dl.hspi, g_dl.pending_erase_addr) == W25Q128_OK) {
            g_dl.pending_erase_addr = 0U;
        }
        /* else: erase failed — retry on next 1 Hz tick at surface */
    }

    /* ── Dive-start detection ── */
    if (!g_dl.dive_active && depth_mm >= DIVE_LOG_START_DEPTH_MM) {
        /* Build PPO snapshot from already-captured snap_ppo/snap_valid */
        uint16_t start_ppo[3];
        for (int i = 0; i < 3; i++) {
            start_ppo[i] = (snap_valid[i] && snap_ppo[i] > 0) ? (uint16_t)snap_ppo[i] : 0U;
        }

        /* Only commit state changes after the flash write succeeds */
        if (entry_write(DIVE_LOG_ENTRY_TYPE_START, depth_cm, 0U,
                        start_ppo, bat_pct, temperature)) {
            g_dl.log_id++;
            g_dl.dive_start_timer    = elapsed_sec;
            g_dl.surface_sec_counter = 0U;
            g_dl.prev_depth_cm       = depth_cm;
            for (int i = 0; i < 3; i++) g_dl.prev_ppo[i] = start_ppo[i];
            g_dl.dive_active         = true;
        }
        return;
    }

    if (!g_dl.dive_active) return;

    /* ── Dive-end: surface timeout ── */
    if (depth_mm < DIVE_LOG_START_DEPTH_MM) {
        g_dl.surface_sec_counter++;
        if (g_dl.surface_sec_counter >= DIVE_LOG_SURFACE_TIMEOUT_SEC) {
            g_dl.dive_active        = false;
            g_dl.surface_sec_counter = 0U;
            meta_save();
        }
        return;
    }

    /* Still submerged — reset surface counter */
    g_dl.surface_sec_counter = 0U;

    /* ── Sample threshold check ── */
    uint32_t depth_delta = (depth_cm > g_dl.prev_depth_cm)
        ? (uint32_t)(depth_cm - g_dl.prev_depth_cm)
        : (uint32_t)(g_dl.prev_depth_cm - depth_cm);
    bool depth_trig = (depth_delta >= DIVE_LOG_DEPTH_THRESHOLD_CM);

    bool ppo_trig = false;
    for (int i = 0; i < 3 && !ppo_trig; i++) {
        if (!snap_valid[i]) continue;
        uint16_t curr = (snap_ppo[i] > 0) ? (uint16_t)snap_ppo[i] : 0U;
        uint32_t delta = (curr > g_dl.prev_ppo[i])
            ? (uint32_t)(curr - g_dl.prev_ppo[i])
            : (uint32_t)(g_dl.prev_ppo[i] - curr);
        if (delta >= DIVE_LOG_PPO_THRESHOLD_MBAR) ppo_trig = true;
    }

    if (!depth_trig && !ppo_trig) return;

    /* ── Write sample ── */
    uint16_t timer_sec = (elapsed_sec >= g_dl.dive_start_timer)
        ? (uint16_t)(elapsed_sec - g_dl.dive_start_timer)  /* truncates at ~18h */
        : 0U;

    uint16_t ppo[3] = {
        (snap_valid[0] && snap_ppo[0] > 0) ? (uint16_t)snap_ppo[0] : 0U,
        (snap_valid[1] && snap_ppo[1] > 0) ? (uint16_t)snap_ppo[1] : 0U,
        (snap_valid[2] && snap_ppo[2] > 0) ? (uint16_t)snap_ppo[2] : 0U,
    };

    if (entry_write(DIVE_LOG_ENTRY_TYPE_SAMPLE, depth_cm, timer_sec,
                    ppo, bat_pct, temperature)) {
        g_dl.prev_depth_cm = depth_cm;
        for (int i = 0; i < 3; i++) g_dl.prev_ppo[i] = ppo[i];
    }
}

/* ============================================================================
 * PUBLIC: GET COUNT
 * ========================================================================= */

uint32_t dive_log_get_count(void)
{
    return g_dl.log_id;
}

/* ============================================================================
 * PUBLIC: ITERATE
 * ========================================================================= */

/* Cap to avoid excessive RAM/time; caller may pass a smaller max_dives. */
#define ITER_CAP 10U

uint32_t dive_log_iterate(dive_log_iter_cb cb, void *user_data, uint32_t max_dives)
{
    if (!g_dl.initialized || cb == NULL || g_dl.entry_count == 0U) return 0U;

    uint32_t limit = (max_dives == 0U || max_dives > ITER_CAP) ? ITER_CAP : max_dives;

    /* Collect dive summaries by scanning forward */
    typedef struct {
        uint32_t log_id;
        uint32_t samples;
        uint16_t max_depth;
        uint16_t duration;
    } dive_sum_t;

    dive_sum_t sums[ITER_CAP];
    uint32_t sum_count = 0U;

    uint8_t  buf[DIVE_LOG_ENTRY_SIZE];
    uint32_t cur_log_id    = 0U;
    uint32_t cur_samples   = 0U;
    uint16_t cur_max_depth = 0U;
    uint16_t cur_duration  = 0U;
    bool     in_dive       = false;

    for (uint32_t off = 0U; off < g_dl.write_offset; off += DIVE_LOG_ENTRY_SIZE) {
        if (w25q128_read(g_dl.hspi, data_off_to_addr(off),
                         buf, DIVE_LOG_ENTRY_SIZE) != W25Q128_OK) break;

        if (buf[0] == DIVE_LOG_ENTRY_TYPE_START) {
            if (in_dive) {
                /* Shift buffer to make room for newest dive (circular keep-last-N) */
                if (sum_count < ITER_CAP) {
                    sums[sum_count].log_id    = cur_log_id;
                    sums[sum_count].samples   = cur_samples;
                    sums[sum_count].max_depth = cur_max_depth;
                    sums[sum_count].duration  = cur_duration;
                    sum_count++;
                } else {
                    /* Slide window: discard oldest, append newest */
                    memmove(&sums[0], &sums[1], (ITER_CAP - 1U) * sizeof(dive_sum_t));
                    sums[ITER_CAP - 1U].log_id    = cur_log_id;
                    sums[ITER_CAP - 1U].samples   = cur_samples;
                    sums[ITER_CAP - 1U].max_depth = cur_max_depth;
                    sums[ITER_CAP - 1U].duration  = cur_duration;
                }
            }
            cur_log_id    = get_u32(&buf[4]);
            cur_samples   = 0U;
            cur_max_depth = get_u16(&buf[8]);
            cur_duration  = 0U;
            in_dive       = true;

        } else if (buf[0] == DIVE_LOG_ENTRY_TYPE_SAMPLE && in_dive) {
            cur_samples++;
            uint16_t d = get_u16(&buf[8]);
            uint16_t t = get_u16(&buf[10]);
            if (d > cur_max_depth) cur_max_depth = d;
            if (t > cur_duration)  cur_duration  = t;
        }
    }

    /* Save the last in-progress dive */
    if (in_dive) {
        if (sum_count < ITER_CAP) {
            sums[sum_count].log_id    = cur_log_id;
            sums[sum_count].samples   = cur_samples;
            sums[sum_count].max_depth = cur_max_depth;
            sums[sum_count].duration  = cur_duration;
            sum_count++;
        } else {
            memmove(&sums[0], &sums[1], (ITER_CAP - 1U) * sizeof(dive_sum_t));
            sums[ITER_CAP - 1U].log_id    = cur_log_id;
            sums[ITER_CAP - 1U].samples   = cur_samples;
            sums[ITER_CAP - 1U].max_depth = cur_max_depth;
            sums[ITER_CAP - 1U].duration  = cur_duration;
            /* sum_count already == ITER_CAP; no increment */
        }
    }

    /* Report newest-first up to limit */
    uint32_t report_count = (sum_count < limit) ? sum_count : limit;
    for (uint32_t i = 0U; i < report_count; i++) {
        dive_sum_t *s = &sums[sum_count - 1U - i];
        cb(s->log_id, s->samples, s->max_depth, s->duration, user_data);
    }
    return report_count;
}

/* ============================================================================
 * PUBLIC: READ SAMPLES
 * ========================================================================= */

uint32_t dive_log_read_samples(uint32_t log_id, uint32_t skip, uint32_t max_count,
                                dive_log_sample_cb cb, void *user_data)
{
    if (!g_dl.initialized || cb == NULL) return 0U;

    uint8_t  buf[DIVE_LOG_ENTRY_SIZE];
    bool     in_target  = false;
    uint32_t found      = 0U;   /* samples encountered in this dive */
    uint32_t reported   = 0U;   /* samples passed to callback       */

    for (uint32_t off = 0U; off < g_dl.write_offset; off += DIVE_LOG_ENTRY_SIZE) {
        if (w25q128_read(g_dl.hspi, data_off_to_addr(off),
                         buf, DIVE_LOG_ENTRY_SIZE) != W25Q128_OK) break;

        if (buf[0] == DIVE_LOG_ENTRY_TYPE_START) {
            uint32_t id = get_u32(&buf[4]);
            if (in_target) break;   /* past the target dive — stop */
            if (id == log_id) in_target = true;

        } else if (buf[0] == DIVE_LOG_ENTRY_TYPE_SAMPLE && in_target) {
            if (found >= skip) {
                if (max_count > 0U && reported >= max_count) break;
                cb((uint32_t)(found - skip),
                   get_u16(&buf[8]),
                   get_u16(&buf[10]),
                   get_u16(&buf[12]),
                   get_u16(&buf[14]),
                   get_u16(&buf[16]),
                   buf[18],
                   (int16_t)get_u16(&buf[20]),
                   user_data);
                reported++;
            }
            found++;
        }
    }

    return reported;
}

/* ============================================================================
 * PUBLIC: ERASE
 * ========================================================================= */

bool dive_log_erase(void)
{
    if (!g_dl.initialized) return false;

    SPI_HandleTypeDef *hspi = g_dl.hspi;   /* save before memset */
    bool ok = true;

    if (w25q128_erase_sector(hspi, DIVE_META_BANK_A) != W25Q128_OK) ok = false;
    if (w25q128_erase_sector(hspi, DIVE_META_BANK_B) != W25Q128_OK) ok = false;

    for (uint32_t addr = DIVE_LOG_DATA_START;
         addr < DIVE_LOG_DATA_START + DIVE_LOG_DATA_SIZE;
         addr += 4096U) {
        if (w25q128_erase_sector(hspi, addr) != W25Q128_OK) ok = false;
    }

    if (ok) {
        memset(&g_dl, 0, sizeof(g_dl));
        g_dl.hspi        = hspi;
        g_dl.initialized = true;
    }
    return ok;
}
