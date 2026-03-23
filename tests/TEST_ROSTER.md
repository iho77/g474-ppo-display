# Test Roster — g474_port Unit Tests

## Overview

**Framework**: Custom native x86 GCC harness — no external dependencies
**Build**: `cd tests && make`
**Run**: `cd tests && make test`
**Compiler**: `gcc -Wall -Werror -Wextra -std=c11 -g -O0 -DUNIT_TEST -lm`

**Run command (Windows with WSL)**:
```
"C:/ST/STM32CubeIDE_1.19.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.100.202601091506/tools/bin/make.exe" -C tests test
```
**Alternative**: From WSL terminal: `cd /mnt/g/stm/g474_port/tests && make test` (using system `make`)

---

## Test Files

| File | Module | Status | Test Count |
|------|--------|--------|------------|
| test_calibration.c | calibration.c | READY | 9 |
| test_drift_tracker.c | drift_tracker.c | READY | 13 |
| test_flash_utils.c | flash_utils.c | READY | 22 |
| test_warning.c | warning.c | READY | 15 |
| test_settings.c | settings_storage.c | READY | 22 |
| test_pressure_sensor.c | pressure_sensor.c | READY | 7 |
| test_ms5837.c | ms5837.c | READY | 7 |

**Total tests: 95**

---

## Modules Excluded (HAL-Dependent)

| Module | Reason |
|--------|--------|
| sensor_diagnostics.c | `sensor_diagnostics_calculate()` calls `flash_calibration_iterate()` / `flash_calibration_get_count()` which iterate SPI flash via `ext_flash_storage`. No pure-logic path. |
| button.c | GPIO + EXTI interrupt-driven. All functions touch HAL_GPIO. |
| st7789v.c | SPI + DMA display driver. No pure logic. |
| w25q128.c | SPI NOR flash driver. Hardware-only. |
| ext_flash_storage.c | W25Q128 SPI operations (sector erase, page program). |
| flash_storage.c | Internal STM32 flash (HAL_FLASH_Program). |
| screen_*.c | LVGL + HAL-dependent. |
| screen_manager.c | LVGL screen switching. |

---

## Functions Under Test

### calibration.c

- [x] `calibrate_sensor`: normal two-point calibration → coefficients written
- [x] `calibrate_sensor`: division by zero (identical raw readings) → NON_CALIBRATED
- [x] `calibrate_sensor`: inverted slope (ppo up, raw down) → NON_CALIBRATED
- [x] `calibrate_sensor`: dead sensor (zero ppo delta) → NON_CALIBRATED
- [x] `calibrate_sensor`: separation +49 counts → NON_CALIBRATED (< 50 minimum)
- [x] `calibrate_sensor`: separation -49 counts → NON_CALIBRATED
- [x] `calibrate_sensor`: exactly 50 count separation → succeeds
- [x] `calibrate_sensor`: calibration points stored in s_cpt[]
- [x] `calibrate_sensor`: coefficients mathematically correct for known linear relation

### drift_tracker.c

- [x] `drift_tracker_init`: all fields zeroed
- [x] `drift_tracker_sample`: 1 sample → count=1, slope=0 (below minimum)
- [x] `drift_tracker_sample`: 2 samples → count=2, slope=0
- [x] `drift_tracker_sample`: 3 flat samples → slope=0
- [x] `drift_tracker_sample`: 3 rising samples → positive slope (+36 mbar/min)
- [x] `drift_tracker_sample`: 3 falling samples → negative slope (-36 mbar/min)
- [x] `drift_tracker_sample`: >18 samples → count saturates at DRIFT_WINDOW_SIZE
- [x] `drift_tracker_sample`: circular buffer wraps correctly after full window
- [x] `drift_get_rate_per_min`: returns stored drift_rate_mbar_per_min
- [x] `drift_tracker_calculate_slope`: corrupted sample_count > window → safe reset
- [x] `drift_tracker_calculate_slope`: constant +1/interval rate → 6 mbar/min output
- [x] `drift_tracker_calculate_slope`: count < 3 → returns 0
- [x] Multiple independent tracker structs do not interfere

### flash_utils.c (pure math only)

**HAL-excluded**: `flash_unlock`, `flash_lock`, `flash_clear_errors`, `flash_page_erase`

- [x] `flash_crc16_ccitt`: "123456789" → 0x29B1 (standard vector)
- [x] `flash_crc16_ccitt`: empty buffer → 0xFFFF (initial seed)
- [x] `flash_crc16_ccitt`: single zero byte → differs from seed
- [x] `flash_crc16_ccitt`: different data → different CRC
- [x] `flash_crc16_ccitt`: same data → same CRC (deterministic)
- [x] `flash_crc32`: "123456789" → 0xCBF43926 (standard vector)
- [x] `flash_crc32`: empty buffer → 0x00000000
- [x] `flash_crc32`: single byte → non-zero
- [x] `flash_crc32`: different data → different CRC
- [x] `flash_crc32`: all-zeros 8 bytes → deterministic
- [x] `flash_make_dword`: basic pack (0x12345678, 0xABCDEF01) → 0xABCDEF0112345678
- [x] `flash_make_dword`: zero both → 0
- [x] `flash_make_dword`: max values → 0xFFFFFFFFFFFFFFFF
- [x] `flash_get_low_word`: extracts lower 32 bits
- [x] `flash_get_high_word`: extracts upper 32 bits
- [x] Round-trip: pack then unpack recovers original low/high words
- [x] `flash_float_to_u32`: 0.0f → 0x00000000
- [x] `flash_float_to_u32`: 1.0f → 0x3F800000 (IEEE 754)
- [x] `flash_float_to_u32`: 0.42f round-trip → bit-for-bit identical
- [x] `flash_u32_to_float`: 0x3F800000 → 1.0f
- [x] Float round-trip: negative value preserved
- [x] Float round-trip: large value preserved

### warning.c

**HAL-excluded**: `warning_apply_style` (LVGL), `vibro_tick_5ms` (TIM PWM), `vibro_hw_start/stop`

- [x] `warning_check_sensor`: invalid sensor index → WARNING_NONE
- [x] `warning_check_sensor`: s_valid[i] == false → WARNING_NONE
- [x] `warning_check_sensor`: PPO in safe zone (700 mbar) → WARNING_NONE
- [x] `warning_check_sensor`: PPO below min (159 < 160) → WARNING_CRITICAL
- [x] `warning_check_sensor`: PPO above max (1401 > 1400) → WARNING_CRITICAL
- [x] `warning_check_sensor`: PPO exactly at min (160) → NOT critical (strict <)
- [x] `warning_check_sensor`: PPO exactly at max (1400) → NOT critical (strict >)
- [x] `warning_check_sensor`: PPO near min (220) → WARNING_NEAR_CRITICAL
- [x] `warning_check_sensor`: PPO near max (1350) → WARNING_NEAR_CRITICAL
- [x] `warning_check_sensor`: PPO 110 above setpoint (810) → WARNING_YELLOW_DRIFT
- [x] `warning_check_sensor`: PPO 110 below setpoint (590) → WARNING_YELLOW_DRIFT
- [x] `warning_check_sensor`: PPO exactly at setpoint (700) → WARNING_NONE
- [x] `warning_check_sensor`: PPO 90 from setpoint (790) → WARNING_NONE (< 100)
- [x] `warning_check_sensor`: active_setpoint=2 → uses ppo_setpoint2
- [x] `warning_check_sensor`: priority — CRITICAL beats NEAR_CRITICAL

### settings_storage.c (pure logic only)

**HAL-excluded**: `settings_save`, `settings_load`, `settings_erase` (delegate to ext_flash_storage)

- [x] `settings_init_defaults`: lcd_brightness = BRIGHTNESS_DEFAULT (100)
- [x] `settings_init_defaults`: min_ppo_threshold = MIN_PPO_DEFAULT (160)
- [x] `settings_init_defaults`: max_ppo_threshold = MAX_PPO_DEFAULT (1400)
- [x] `settings_init_defaults`: delta_ppo_threshold = DELTA_PPO_DEFAULT (100)
- [x] `settings_init_defaults`: warning_ppo_threshold = WARNING_PPO_DEFAULT (100)
- [x] `settings_init_defaults`: ppo_setpoint1/2 = 700/1100
- [x] `settings_validate`: in-range defaults → unchanged
- [x] `settings_validate`: brightness below BRIGHTNESS_MIN → clamped to min
- [x] `settings_validate`: brightness above BRIGHTNESS_MAX → clamped to max
- [x] `settings_validate`: brightness at BRIGHTNESS_MIN → unchanged
- [x] `settings_validate`: min_ppo below MIN_PPO_MIN → clamped
- [x] `settings_validate`: min_ppo above MIN_PPO_MAX → clamped
- [x] `settings_validate`: max_ppo below MAX_PPO_MIN → clamped
- [x] `settings_validate`: max_ppo above MAX_PPO_MAX → clamped
- [x] `settings_validate`: delta_ppo below DELTA_PPO_MIN → clamped
- [x] `settings_validate`: delta_ppo above DELTA_PPO_MAX → clamped
- [x] `settings_validate`: warning_ppo below WARNING_PPO_MIN → clamped
- [x] `settings_validate`: warning_ppo above WARNING_PPO_MAX → clamped
- [x] `settings_validate`: setpoint1 below SETPOINT1_MIN → clamped
- [x] `settings_validate`: setpoint1 above SETPOINT1_MAX → clamped
- [x] `settings_validate`: setpoint2 below SETPOINT2_MIN → clamped
- [x] `settings_validate`: setpoint2 above SETPOINT2_MAX → clamped

### pressure_sensor.c

- [x] `pressure_sensor_calculate_depth_mm`: same pressure → 0 mm depth
- [x] `pressure_sensor_calculate_depth_mm`: 10m depth equivalent → -9950 mm
- [x] `pressure_sensor_calculate_depth_mm`: pressure below surface → positive mm
- [x] `pressure_sensor_calculate_depth_mm`: exact coefficient (10000 delta → 995 mm)
- [x] `pressure_sensor_calculate_depth_mm`: large positive delta
- [x] `pressure_sensor_calculate_depth_mm`: large negative delta (symmetric)
- [x] `pressure_sensor_init`: populates sensor_data.pressure with correct defaults

### ms5837.c (ms5837_calculate only)

**HAL-excluded**: `ms5837_reset`, `ms5837_read_prom`, `ms5837_read_adc`,
`ms5837_convert_d1/d2`, `ms5837_init`, `ms5837_read_pt`

- [x] `ms5837_calculate`: returns MS5837_OK for valid inputs
- [x] `ms5837_calculate`: stores D1_raw and D2_raw in data struct
- [x] `ms5837_calculate`: pressure_mbar in plausible range (0–35000)
- [x] `ms5837_calculate`: temperature_c100 in plausible range (-2000–8500)
- [x] `ms5837_calculate`: low-temperature path (TEMP < 2000) → compensation applied
- [x] `ms5837_calculate`: high-temperature path (TEMP >= 2000) → compensation applied
- [x] `ms5837_calculate`: all-zero coefficients → no crash, returns OK

---

## Mock Files

| File | Purpose |
|------|---------|
| `mocks/mock_stm32.h` | HAL type stubs (TIM, I2C, SPI handles; IRQ macros; LL ADC macros) |
| `mocks/stm32g4xx_hal.h` | Forwarding stub → redirects `#include "stm32g4xx_hal.h"` to mock_stm32.h |
| `mocks/stm32g4xx_ll_adc.h` | LL ADC stub → redirects to mock_stm32.h |
| `mocks/lvgl.h` | Minimal LVGL type stubs (lv_obj_t, lv_color_t, style setters) |
| `mocks/main.h` | main.h stub → provides htim5 extern via mock_stm32.h |
| `mocks/screen_manager.h` | screen_manager_startup_complete() no-op stub |
| `mocks/ext_flash_storage.h` | ext_settings_save/load/erase stubs (inline, no-op) |
| `mocks/flash_storage.h` | flash_calibration_get_count/iterate stubs (return 0) |
| `mocks/mock_sensor_data.h` | Declares reset_sensor_data() and htim5 |
| `mocks/mock_sensor_data.c` | Defines sensor_data, active_setpoint, htim5; implements reset_sensor_data() |

---

## Known Limitations

1. **sensor_diagnostics_calculate()** cannot be tested without a real flash log.
   The static helpers (`calculate_mean`, `calculate_stddev`, `calc_sensitivity_ratio`,
   etc.) are `static` and not accessible from outside the TU.
   Workaround: expose them via `#ifdef UNIT_TEST` blocks if needed in future.

2. **sensor.c filter functions** (`filter_step_sensor`, `median_int32`,
   `mad_int32_q0`, `filter_step_sensor_two_stage`) are `static inline` or
   `static` and depend on module-level static state. They are compiled into the
   test binary but not directly exercised in this suite iteration.

3. **warning vibro state machine** (`warning_trigger_vibration`, `vibro_tick_5ms`,
   `vibro_acknowledge`, `vibro_is_active`) is compiled in with HAL TIM stubs.
   Behavioral timing tests (motor on/off sequencing) require either precise timing
   or a deeper mock of `vibro_hw_start/stop` — deferred.

---

## Changelog

| Date | Change |
|------|--------|
| 2026-03-22 | Initial test suite created — 95 tests across 7 modules, all passing |
