/**
 * @file ext_flash_storage.c
 * @brief External W25Q128 flash storage implementation.
 *
 * Settings: Ping-Pong (Bank A / Bank B) in sectors 0–1.
 * Calibration: Append-only log in sectors 2–17 (64KB, 1280 entries).
 *
 * Entry formats:
 *
 * Settings (28 bytes):
 *   [0-3]   magic    = EXT_SETTINGS_MAGIC
 *   [4-7]   sequence (uint32_t, little-endian)
 *   [8]     lcd_brightness
 *   [9]     pad = 0
 *   [10-11] min_ppo_threshold
 *   [12-13] max_ppo_threshold
 *   [14-15] delta_ppo_threshold
 *   [16-17] warning_ppo_threshold
 *   [18-19] ppo_setpoint1
 *   [20-21] ppo_setpoint2
 *   [22-23] pad = 0
 *   [24-27] crc32 over bytes [0-23] with crc field = 0
 *
 * Calibration (48 bytes):
 *   [0-3]   magic    = EXT_CAL_MAGIC
 *   [4-7]   sequence (uint32_t, little-endian)
 *   [8-11]  crc32 over all 48 bytes with crc field = 0
 *   [12]    states[0]
 *   [13]    states[1]
 *   [14]    states[2]
 *   [15]    pad = 0
 *   [16-19] slope[0] (float bit pattern)
 *   [20-23] slope[1]
 *   [24-27] slope[2]
 *   [28-31] intercept[0]
 *   [32-35] intercept[1]
 *   [36-39] intercept[2]
 *   [40-41] pt1.raw
 *   [42-43] pt1.ppo
 *   [44-45] pt2.raw
 *   [46-47] pt2.ppo
 */

#include "ext_flash_storage.h"
#include "w25q128.h"
#include "flash_utils.h"
#include "sensor.h"
#include <string.h>

extern volatile sensor_data_t sensor_data;

/* ── Module state (10 bytes total) ── */
static SPI_HandleTypeDef *g_hspi       = NULL;   /* 4 bytes */
static uint32_t           g_cal_write_offset = 0U; /* 4 bytes — offset within cal zone */
static uint8_t            g_active_bank = 0U;    /* 0 = A, 1 = B */
static bool               g_initialized = false; /* 1 byte  */

/* ============================================================================
 * HELPERS: byte-level little-endian read/write
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
 * CALIBRATION: address arithmetic
 * ========================================================================= */

/**
 * @brief Convert zone-relative offset to absolute flash address.
 */
static inline uint32_t cal_off_to_addr(uint32_t offset)
{
    return EXT_CAL_SECTOR_FIRST + offset;
}

/**
 * @brief Advance zone offset past the entry just written at 'offset'.
 *        Entries pack 5-per-page (0,48,96,144,192). After the 5th entry
 *        (page offset 192), skip to the next page boundary.
 */
static uint32_t cal_advance_offset(uint32_t offset)
{
    if ((offset % 256U) == 192U) {
        return offset + 64U;   /* skip to next page start */
    }
    return offset + EXT_CAL_ENTRY_SIZE;
}

/* ============================================================================
 * CALIBRATION: boot scan
 * ========================================================================= */

static void cal_scan_write_ptr(void)
{
    uint8_t magic_buf[4];

    for (uint32_t off = 0U; off < EXT_CAL_ZONE_SIZE; off = cal_advance_offset(off)) {
        if (w25q128_read(g_hspi, cal_off_to_addr(off), magic_buf, 4U) != W25Q128_OK) {
            /* SPI error — stop scan, use current offset as write pointer */
            g_cal_write_offset = off;
            return;
        }
        if (get_u32(magic_buf) == 0xFFFFFFFFUL) {
            /* Found first blank slot — this is where the next write goes */
            g_cal_write_offset = off;
            return;
        }
    }
    /* No blank slot found (log full or contains garbage) — wrap to start.
     * The per-sector erase in ext_cal_save will clean sector 2 before first write. */
    g_cal_write_offset = 0U;
}

/* ============================================================================
 * SETTINGS: pack / unpack
 * ========================================================================= */

static void settings_pack(uint8_t buf[EXT_SETTINGS_ENTRY_SIZE],
                          const settings_data_t *s, uint32_t seq)
{
    memset(buf, 0, EXT_SETTINGS_ENTRY_SIZE);
    put_u32(&buf[0],  EXT_SETTINGS_MAGIC);
    put_u32(&buf[4],  seq);
    buf[8]  = s->lcd_brightness;
    buf[9]  = 0U;
    put_u16(&buf[10], s->min_ppo_threshold);
    put_u16(&buf[12], s->max_ppo_threshold);
    put_u16(&buf[14], s->delta_ppo_threshold);
    put_u16(&buf[16], s->warning_ppo_threshold);
    put_u16(&buf[18], s->ppo_setpoint1);
    put_u16(&buf[20], s->ppo_setpoint2);
    /* bytes [22-23] pad = 0; bytes [24-27] crc placeholder = 0 */

    uint32_t crc = flash_crc32(buf, 24U);
    put_u32(&buf[24], crc);
}

static bool settings_unpack(const uint8_t buf[EXT_SETTINGS_ENTRY_SIZE],
                             settings_data_t *s)
{
    if (get_u32(&buf[0]) != EXT_SETTINGS_MAGIC) {
        return false;
    }

    /* Verify CRC over bytes [0-23] */
    uint8_t tmp[EXT_SETTINGS_ENTRY_SIZE];
    memcpy(tmp, buf, EXT_SETTINGS_ENTRY_SIZE);
    put_u32(&tmp[24], 0U); /* zero crc field */
    if (flash_crc32(tmp, 24U) != get_u32(&buf[24])) {
        return false;
    }

    s->lcd_brightness       = buf[8];
    s->min_ppo_threshold    = get_u16(&buf[10]);
    s->max_ppo_threshold    = get_u16(&buf[12]);
    s->delta_ppo_threshold  = get_u16(&buf[14]);
    s->warning_ppo_threshold = get_u16(&buf[16]);
    s->ppo_setpoint1        = get_u16(&buf[18]);
    s->ppo_setpoint2        = get_u16(&buf[20]);
    return true;
}

/* ── Read one settings bank, return sequence if valid, 0 on failure ── */
static bool settings_read_bank(uint32_t bank_addr, uint8_t out[EXT_SETTINGS_ENTRY_SIZE],
                                uint32_t *seq_out)
{
    if (w25q128_read(g_hspi, bank_addr, out, EXT_SETTINGS_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }
    if (get_u32(&out[0]) != EXT_SETTINGS_MAGIC) {
        return false;
    }
    uint8_t tmp[EXT_SETTINGS_ENTRY_SIZE];
    memcpy(tmp, out, EXT_SETTINGS_ENTRY_SIZE);
    put_u32(&tmp[24], 0U);
    if (flash_crc32(tmp, 24U) != get_u32(&out[24])) {
        return false;
    }
    *seq_out = get_u32(&out[4]);
    return true;
}

/* ============================================================================
 * CALIBRATION: pack / unpack
 * ========================================================================= */

static void cal_pack_entry(uint8_t buf[EXT_CAL_ENTRY_SIZE], uint32_t seq)
{
    memset(buf, 0, EXT_CAL_ENTRY_SIZE);
    put_u32(&buf[0],  EXT_CAL_MAGIC);
    put_u32(&buf[4],  seq);
    /* bytes [8-11] crc placeholder = 0 */
    buf[12] = (uint8_t)sensor_data.os_s_cal_state[0];
    buf[13] = (uint8_t)sensor_data.os_s_cal_state[1];
    buf[14] = (uint8_t)sensor_data.os_s_cal_state[2];
    buf[15] = 0U;
    put_u32(&buf[16], flash_float_to_u32(sensor_data.s_cal[0][0]));
    put_u32(&buf[20], flash_float_to_u32(sensor_data.s_cal[0][1]));
    put_u32(&buf[24], flash_float_to_u32(sensor_data.s_cal[0][2]));
    put_u32(&buf[28], flash_float_to_u32(sensor_data.s_cal[1][0]));
    put_u32(&buf[32], flash_float_to_u32(sensor_data.s_cal[1][1]));
    put_u32(&buf[36], flash_float_to_u32(sensor_data.s_cal[1][2]));
    put_u16(&buf[40], sensor_data.s_cpt[0].raw);
    put_u16(&buf[42], sensor_data.s_cpt[0].ppo);
    put_u16(&buf[44], sensor_data.s_cpt[1].raw);
    put_u16(&buf[46], sensor_data.s_cpt[1].ppo);

    uint32_t crc = flash_crc32(buf, EXT_CAL_ENTRY_SIZE);
    put_u32(&buf[8], crc);
}

static bool cal_unpack_entry(const uint8_t buf[EXT_CAL_ENTRY_SIZE])
{
    if (get_u32(&buf[0]) != EXT_CAL_MAGIC) {
        return false;
    }
    /* Verify CRC: zero field [8-11] and recalculate */
    uint8_t tmp[EXT_CAL_ENTRY_SIZE];
    memcpy(tmp, buf, EXT_CAL_ENTRY_SIZE);
    put_u32(&tmp[8], 0U);
    if (flash_crc32(tmp, EXT_CAL_ENTRY_SIZE) != get_u32(&buf[8])) {
        return false;
    }

    sensor_data.os_s_cal_state[0] = (sensor_cal_state)buf[12];
    sensor_data.os_s_cal_state[1] = (sensor_cal_state)buf[13];
    sensor_data.os_s_cal_state[2] = (sensor_cal_state)buf[14];
    sensor_data.s_cal[0][0] = flash_u32_to_float(get_u32(&buf[16]));
    sensor_data.s_cal[0][1] = flash_u32_to_float(get_u32(&buf[20]));
    sensor_data.s_cal[0][2] = flash_u32_to_float(get_u32(&buf[24]));
    sensor_data.s_cal[1][0] = flash_u32_to_float(get_u32(&buf[28]));
    sensor_data.s_cal[1][1] = flash_u32_to_float(get_u32(&buf[32]));
    sensor_data.s_cal[1][2] = flash_u32_to_float(get_u32(&buf[36]));
    sensor_data.s_cpt[0].raw = get_u16(&buf[40]);
    sensor_data.s_cpt[0].ppo = get_u16(&buf[42]);
    sensor_data.s_cpt[1].raw = get_u16(&buf[44]);
    sensor_data.s_cpt[1].ppo = get_u16(&buf[46]);
    return true;
}

/* ── Validate magic + CRC of a raw buffer without writing to sensor_data ── */
static bool cal_entry_valid(const uint8_t buf[EXT_CAL_ENTRY_SIZE])
{
    if (get_u32(&buf[0]) != EXT_CAL_MAGIC) {
        return false;
    }
    uint8_t tmp[EXT_CAL_ENTRY_SIZE];
    memcpy(tmp, buf, EXT_CAL_ENTRY_SIZE);
    put_u32(&tmp[8], 0U);
    return (flash_crc32(tmp, EXT_CAL_ENTRY_SIZE) == get_u32(&buf[8]));
}

/* ============================================================================
 * PUBLIC: INIT
 * ========================================================================= */

bool ext_flash_storage_init(SPI_HandleTypeDef *hspi)
{
    g_hspi        = hspi;
    g_initialized = false;
    g_active_bank = 0U;
    g_cal_write_offset = 0U;

    if (w25q128_init(hspi) != W25Q128_OK) {
        return false;
    }

    /* Determine active settings bank */
    uint8_t buf_a[EXT_SETTINGS_ENTRY_SIZE];
    uint8_t buf_b[EXT_SETTINGS_ENTRY_SIZE];
    uint32_t seq_a = 0U, seq_b = 0U;
    bool valid_a = settings_read_bank(EXT_SETTINGS_BANK_A, buf_a, &seq_a);
    bool valid_b = settings_read_bank(EXT_SETTINGS_BANK_B, buf_b, &seq_b);

    if (valid_a && valid_b) {
        g_active_bank = (seq_b > seq_a) ? 1U : 0U;
    } else if (valid_b) {
        g_active_bank = 1U;
    } else {
        g_active_bank = 0U;
    }

    /* Scan calibration log to find write pointer (non-fatal) */
    cal_scan_write_ptr();

    g_initialized = true;
    return true;
}

/* ============================================================================
 * PUBLIC: SETTINGS
 * ========================================================================= */

bool ext_settings_save(const settings_data_t *settings)
{
    if (!g_initialized || (settings == NULL)) {
        return false;
    }

    /* Find highest existing sequence */
    uint8_t rbuf[EXT_SETTINGS_ENTRY_SIZE];
    uint32_t seq_a = 0U, seq_b = 0U;
    bool valid_a = settings_read_bank(EXT_SETTINGS_BANK_A, rbuf, &seq_a);
    bool valid_b = settings_read_bank(EXT_SETTINGS_BANK_B, rbuf, &seq_b);

    uint32_t highest_seq = 0U;
    if (valid_a && valid_b) {
        highest_seq = (seq_a > seq_b) ? seq_a : seq_b;
    } else if (valid_a) {
        highest_seq = seq_a;
    } else if (valid_b) {
        highest_seq = seq_b;
    }
    uint32_t next_seq = highest_seq + 1U;
    if (next_seq == 0U) next_seq = 1U; /* prevent wrap to 0 */

    /* Write to inactive bank */
    uint8_t wbuf[EXT_SETTINGS_ENTRY_SIZE];
    settings_pack(wbuf, settings, next_seq);

    uint32_t inactive_addr = (g_active_bank == 0U) ? EXT_SETTINGS_BANK_B : EXT_SETTINGS_BANK_A;

    /* Erase inactive bank before writing */
    if (w25q128_erase_sector(g_hspi, inactive_addr) != W25Q128_OK) {
        return false;
    }

    if (w25q128_write_page(g_hspi, inactive_addr, wbuf, EXT_SETTINGS_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }

    /* Verify read-back */
    uint8_t verify[EXT_SETTINGS_ENTRY_SIZE];
    if (w25q128_read(g_hspi, inactive_addr, verify, EXT_SETTINGS_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }
    if (memcmp(wbuf, verify, EXT_SETTINGS_ENTRY_SIZE) != 0) {
        return false; /* active bank still intact */
    }

    /* Switch active bank; erase old active bank */
    uint32_t old_active_addr = (g_active_bank == 0U) ? EXT_SETTINGS_BANK_A : EXT_SETTINGS_BANK_B;
    g_active_bank ^= 1U;
    (void)w25q128_erase_sector(g_hspi, old_active_addr);

    return true;
}

bool ext_settings_load(settings_data_t *settings)
{
    if (!g_initialized || (settings == NULL)) {
        return false;
    }

    uint8_t buf_a[EXT_SETTINGS_ENTRY_SIZE];
    uint8_t buf_b[EXT_SETTINGS_ENTRY_SIZE];
    uint32_t seq_a = 0U, seq_b = 0U;
    bool valid_a = settings_read_bank(EXT_SETTINGS_BANK_A, buf_a, &seq_a);
    bool valid_b = settings_read_bank(EXT_SETTINGS_BANK_B, buf_b, &seq_b);

    const uint8_t *best = NULL;
    if (valid_a && valid_b) {
        best = (seq_b > seq_a) ? buf_b : buf_a;
    } else if (valid_a) {
        best = buf_a;
    } else if (valid_b) {
        best = buf_b;
    } else {
        return false;
    }

    return settings_unpack(best, settings);
}

void ext_settings_erase(void)
{
    if (!g_initialized) {
        return;
    }
    (void)w25q128_erase_sector(g_hspi, EXT_SETTINGS_BANK_A);
    (void)w25q128_erase_sector(g_hspi, EXT_SETTINGS_BANK_B);
    g_active_bank = 0U;
}

/* ============================================================================
 * PUBLIC: CALIBRATION
 * ========================================================================= */

bool ext_cal_save(void)
{
    if (!g_initialized) {
        return false;
    }

    /* Wrap write pointer if it reached end of zone */
    if (g_cal_write_offset >= EXT_CAL_ZONE_SIZE) {
        g_cal_write_offset = 0U;
    }

    /* Erase the current 4KB sector when starting a new one.
     * This handles: first-ever write (offset 0), log wrap, and any sector
     * that may contain stale data from a previous firmware version. */
    if ((g_cal_write_offset % W25Q128_SECTOR_SIZE) == 0U) {
        uint32_t sector_addr = EXT_CAL_SECTOR_FIRST + g_cal_write_offset;
        if (w25q128_erase_sector(g_hspi, sector_addr) != W25Q128_OK) {
            return false;
        }
    }

    /* Determine next sequence number */
    uint32_t next_seq = 1U;
    if (g_cal_write_offset > 0U) {
        /* Read the previous entry to get its sequence */
        uint32_t prev_off;
        uint32_t page_off = g_cal_write_offset % 256U;
        if (page_off == 0U) {
            /* Previous was at offset 192 of the prior page */
            prev_off = g_cal_write_offset - 64U;  /* 256-64=192 relative */
        } else {
            prev_off = g_cal_write_offset - EXT_CAL_ENTRY_SIZE;
        }
        uint8_t prev_buf[EXT_CAL_ENTRY_SIZE];
        if (w25q128_read(g_hspi, cal_off_to_addr(prev_off), prev_buf, EXT_CAL_ENTRY_SIZE) == W25Q128_OK) {
            if (cal_entry_valid(prev_buf)) {
                next_seq = get_u32(&prev_buf[4]) + 1U;
                if (next_seq == 0U) next_seq = 1U;
            }
        }
    }

    uint8_t buf[EXT_CAL_ENTRY_SIZE];
    cal_pack_entry(buf, next_seq);

    uint32_t write_addr = cal_off_to_addr(g_cal_write_offset);
    if (w25q128_write_page(g_hspi, write_addr, buf, EXT_CAL_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }

    g_cal_write_offset = cal_advance_offset(g_cal_write_offset);
    return true;
}

bool ext_cal_load(void)
{
    if (!g_initialized) {
        return false;
    }

    bool found = false;
    uint32_t best_seq = 0U;
    uint32_t best_off = 0U;

    for (uint32_t off = 0U; off < g_cal_write_offset; off = cal_advance_offset(off)) {
        uint8_t buf[EXT_CAL_ENTRY_SIZE];
        if (w25q128_read(g_hspi, cal_off_to_addr(off), buf, EXT_CAL_ENTRY_SIZE) != W25Q128_OK) {
            continue;
        }
        if (!cal_entry_valid(buf)) {
            continue;
        }
        uint32_t seq = get_u32(&buf[4]);
        if (!found || seq > best_seq) {
            best_seq = seq;
            best_off = off;
            found = true;
        }
    }

    if (!found) {
        return false;
    }

    uint8_t best_buf[EXT_CAL_ENTRY_SIZE];
    if (w25q128_read(g_hspi, cal_off_to_addr(best_off), best_buf, EXT_CAL_ENTRY_SIZE) != W25Q128_OK) {
        return false;
    }
    return cal_unpack_entry(best_buf);
}

void ext_cal_erase(void)
{
    if (!g_initialized) {
        return;
    }
    for (uint32_t s = 0U; s < EXT_CAL_SECTOR_COUNT; s++) {
        uint32_t sector_addr = EXT_CAL_SECTOR_FIRST + (uint32_t)(s * 4096U);
        (void)w25q128_erase_sector(g_hspi, sector_addr);
    }
    g_cal_write_offset = 0U;
}

uint32_t ext_cal_get_count(void)
{
    if (!g_initialized) {
        return 0U;
    }
    uint32_t count = 0U;
    for (uint32_t off = 0U; off < g_cal_write_offset; off = cal_advance_offset(off)) {
        uint8_t magic_buf[4];
        if (w25q128_read(g_hspi, cal_off_to_addr(off), magic_buf, 4U) != W25Q128_OK) {
            break;
        }
        if (get_u32(magic_buf) == EXT_CAL_MAGIC) {
            count++;
        }
    }
    return count;
}

uint32_t ext_cal_iterate(flash_calib_iterator_cb cb, void *ud, uint32_t max)
{
    if (!g_initialized || (cb == NULL)) {
        return 0U;
    }

    /* Collect valid entries (sequence + offset) — bounded by write ptr */
    typedef struct { uint32_t offset; uint32_t seq; } entry_ref_t;

    /* Maximum number of entries we can hold for sort: cap at 64 to limit stack */
    #define EXT_CAL_ITER_MAX_SORT 64U

    static entry_ref_t valid[EXT_CAL_ITER_MAX_SORT]; /* static: avoids 512-byte stack alloc on hot path */
    uint32_t valid_count = 0U;

    for (uint32_t off = 0U; off < g_cal_write_offset; off = cal_advance_offset(off)) {
        if (valid_count >= EXT_CAL_ITER_MAX_SORT) {
            break;
        }
        uint8_t buf[EXT_CAL_ENTRY_SIZE];
        if (w25q128_read(g_hspi, cal_off_to_addr(off), buf, EXT_CAL_ENTRY_SIZE) != W25Q128_OK) {
            continue;
        }
        if (!cal_entry_valid(buf)) {
            continue;
        }
        valid[valid_count].offset = off;
        valid[valid_count].seq    = get_u32(&buf[4]);
        valid_count++;
    }

    if (valid_count == 0U) {
        return 0U;
    }

    /* Bubble sort descending by sequence (newest first) */
    for (uint32_t i = 0U; i < valid_count - 1U; i++) {
        for (uint32_t j = 0U; j < valid_count - i - 1U; j++) {
            if (valid[j].seq < valid[j + 1U].seq) {
                entry_ref_t tmp = valid[j];
                valid[j]       = valid[j + 1U];
                valid[j + 1U]  = tmp;
            }
        }
    }

    uint32_t limit = (max == 0U || max > valid_count) ? valid_count : max;
    uint32_t processed = 0U;

    for (uint32_t i = 0U; i < limit; i++) {
        uint8_t buf[EXT_CAL_ENTRY_SIZE];
        if (w25q128_read(g_hspi, cal_off_to_addr(valid[i].offset), buf, EXT_CAL_ENTRY_SIZE) != W25Q128_OK) {
            continue;
        }
        if (!cal_entry_valid(buf)) {
            continue;
        }

        float slopes[3];
        float intercepts[3];
        slopes[0]     = flash_u32_to_float(get_u32(&buf[16]));
        slopes[1]     = flash_u32_to_float(get_u32(&buf[20]));
        slopes[2]     = flash_u32_to_float(get_u32(&buf[24]));
        intercepts[0] = flash_u32_to_float(get_u32(&buf[28]));
        intercepts[1] = flash_u32_to_float(get_u32(&buf[32]));
        intercepts[2] = flash_u32_to_float(get_u32(&buf[36]));

        cb(valid[i].seq, slopes, intercepts, ud);
        processed++;
    }

    return processed;
}
