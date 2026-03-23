/**
 * @file test_flash_utils.c
 * @brief Unit tests for flash_utils.c — pure math functions only
 *
 * HAL-dependent functions (flash_unlock, flash_lock, flash_clear_errors,
 * flash_page_erase) are excluded — they call HAL_FLASH_* and have no
 * pure-logic path to test.
 *
 * Tested functions:
 *  flash_crc16_ccitt  — CRC-16/CCITT (poly 0x1021, seed 0xFFFF)
 *  flash_crc32        — CRC-32 ISO 3309 (poly 0xEDB88320, reflected)
 *  flash_make_dword   — pack (low, high) → uint64_t
 *  flash_get_low_word — extract lower 32 bits
 *  flash_get_high_word— extract upper 32 bits
 *  flash_float_to_u32 — float bit-pattern → uint32_t
 *  flash_u32_to_float — uint32_t → float bit-pattern
 *
 * Known-good CRC vectors:
 *  CRC-16/CCITT: "123456789" → 0x29B1
 *  CRC-32:       "123456789" → 0xCBF43926
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "flash_utils.h"

extern int g_passed;
extern int g_failed;
extern const char *g_current_test;

#define RUN_TEST(test_func)                                             \
    do {                                                                \
        g_current_test = #test_func;                                   \
        printf("  %-60s ", #test_func);                                \
        fflush(stdout);                                                 \
        test_func();                                                    \
        printf("PASS\n");                                               \
        g_passed++;                                                     \
    } while (0)

/* ── CRC-16 tests ─────────────────────────────────────────────────────────── */

static void test_crc16_known_vector(void) {
    /* "123456789" → 0x29B1 (CRC-16/CCITT standard vector) */
    const uint8_t data[] = "123456789";
    uint16_t crc = flash_crc16_ccitt(data, 9);
    assert(crc == 0x29B1u);
}

static void test_crc16_empty_buffer(void) {
    /* Zero bytes → initial CRC = 0xFFFF */
    uint16_t crc = flash_crc16_ccitt(NULL, 0);
    assert(crc == 0xFFFFu);
}

static void test_crc16_single_byte_zero(void) {
    const uint8_t data[] = {0x00};
    uint16_t crc = flash_crc16_ccitt(data, 1);
    /* 0xFF00 XOR loop over 8 bits with poly 0x1021 when bit7 set */
    assert(crc != 0xFFFFu);  /* Must differ from initial seed */
}

static void test_crc16_different_data_different_crc(void) {
    const uint8_t d1[] = {0x01, 0x02, 0x03};
    const uint8_t d2[] = {0x01, 0x02, 0x04};  /* last byte differs */
    uint16_t c1 = flash_crc16_ccitt(d1, 3);
    uint16_t c2 = flash_crc16_ccitt(d2, 3);
    assert(c1 != c2);
}

static void test_crc16_same_data_same_crc(void) {
    const uint8_t d[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint16_t c1 = flash_crc16_ccitt(d, 4);
    uint16_t c2 = flash_crc16_ccitt(d, 4);
    assert(c1 == c2);
}

/* ── CRC-32 tests ─────────────────────────────────────────────────────────── */

static void test_crc32_known_vector(void) {
    /* "123456789" → 0xCBF43926 (CRC-32 ISO 3309 standard vector) */
    const uint8_t data[] = "123456789";
    uint32_t crc = flash_crc32(data, 9);
    assert(crc == 0xCBF43926UL);
}

static void test_crc32_empty_buffer(void) {
    /* Zero bytes: crc = 0xFFFFFFFF XOR 0xFFFFFFFF = 0 */
    uint32_t crc = flash_crc32(NULL, 0);
    assert(crc == 0x00000000UL);
}

static void test_crc32_single_byte(void) {
    const uint8_t data[] = {0x00};
    uint32_t crc = flash_crc32(data, 1);
    assert(crc != 0x00000000UL);
}

static void test_crc32_different_data_different_crc(void) {
    const uint8_t d1[] = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t d2[] = {0xDE, 0xAD, 0xBE, 0xF0};
    uint32_t c1 = flash_crc32(d1, 4);
    uint32_t c2 = flash_crc32(d2, 4);
    assert(c1 != c2);
}

static void test_crc32_all_zeros_eight_bytes(void) {
    const uint8_t d[8] = {0};
    uint32_t c1 = flash_crc32(d, 8);
    uint32_t c2 = flash_crc32(d, 8);
    assert(c1 == c2);          /* Deterministic */
    assert(c1 != 0xFFFFFFFFUL); /* Not seed */
}

/* ── uint64_t packing tests ───────────────────────────────────────────────── */

static void test_make_dword_basic(void) {
    uint64_t dw = flash_make_dword(0x12345678UL, 0xABCDEF01UL);
    assert(dw == 0xABCDEF0112345678ULL);
}

static void test_make_dword_zero_both(void) {
    uint64_t dw = flash_make_dword(0, 0);
    assert(dw == 0ULL);
}

static void test_make_dword_max_values(void) {
    uint64_t dw = flash_make_dword(0xFFFFFFFFUL, 0xFFFFFFFFUL);
    assert(dw == 0xFFFFFFFFFFFFFFFFULL);
}

static void test_get_low_word_basic(void) {
    uint64_t dw = 0xABCDEF0112345678ULL;
    assert(flash_get_low_word(dw) == 0x12345678UL);
}

static void test_get_high_word_basic(void) {
    uint64_t dw = 0xABCDEF0112345678ULL;
    assert(flash_get_high_word(dw) == 0xABCDEF01UL);
}

static void test_roundtrip_pack_unpack(void) {
    uint32_t lo = 0xDEADBEEFUL;
    uint32_t hi = 0xCAFEBABEUL;
    uint64_t dw = flash_make_dword(lo, hi);
    assert(flash_get_low_word(dw) == lo);
    assert(flash_get_high_word(dw) == hi);
}

/* ── Float ↔ uint32_t conversion tests ───────────────────────────────────── */

static void test_float_to_u32_zero(void) {
    uint32_t u = flash_float_to_u32(0.0f);
    assert(u == 0x00000000UL);
}

static void test_float_to_u32_one(void) {
    /* IEEE 754: 1.0f = 0x3F800000 */
    uint32_t u = flash_float_to_u32(1.0f);
    assert(u == 0x3F800000UL);
}

static void test_float_to_u32_known_value(void) {
    /* 0.42f: verify round-trip preserves bit pattern */
    float val = 0.42f;
    uint32_t u = flash_float_to_u32(val);
    float back = flash_u32_to_float(u);
    /* Must be bit-for-bit identical */
    assert(back == val);
}

static void test_u32_to_float_one(void) {
    float f = flash_u32_to_float(0x3F800000UL);
    assert(f == 1.0f);
}

static void test_float_roundtrip_negative(void) {
    float val = -3.14159f;
    uint32_t u = flash_float_to_u32(val);
    float back = flash_u32_to_float(u);
    assert(back == val);
}

static void test_float_roundtrip_large(void) {
    float val = 1234567.89f;
    uint32_t u = flash_float_to_u32(val);
    float back = flash_u32_to_float(u);
    assert(back == val);
}

/* ── Runner ───────────────────────────────────────────────────────────────── */

void run_flash_utils_tests(void) {
    printf("\n[FLASH UTILS TESTS - flash_utils.c (pure math only)]\n");
    /* CRC-16 */
    RUN_TEST(test_crc16_known_vector);
    RUN_TEST(test_crc16_empty_buffer);
    RUN_TEST(test_crc16_single_byte_zero);
    RUN_TEST(test_crc16_different_data_different_crc);
    RUN_TEST(test_crc16_same_data_same_crc);
    /* CRC-32 */
    RUN_TEST(test_crc32_known_vector);
    RUN_TEST(test_crc32_empty_buffer);
    RUN_TEST(test_crc32_single_byte);
    RUN_TEST(test_crc32_different_data_different_crc);
    RUN_TEST(test_crc32_all_zeros_eight_bytes);
    /* Packing */
    RUN_TEST(test_make_dword_basic);
    RUN_TEST(test_make_dword_zero_both);
    RUN_TEST(test_make_dword_max_values);
    RUN_TEST(test_get_low_word_basic);
    RUN_TEST(test_get_high_word_basic);
    RUN_TEST(test_roundtrip_pack_unpack);
    /* Float conversion */
    RUN_TEST(test_float_to_u32_zero);
    RUN_TEST(test_float_to_u32_one);
    RUN_TEST(test_float_to_u32_known_value);
    RUN_TEST(test_u32_to_float_one);
    RUN_TEST(test_float_roundtrip_negative);
    RUN_TEST(test_float_roundtrip_large);
}
