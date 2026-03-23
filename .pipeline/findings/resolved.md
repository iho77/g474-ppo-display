# Resolved Findings

## Resolution Batch — 2026-03-23

**Scope:** All modules | **Severity floor:** All (CRITICAL → LOW)

### Fixed

| # | Finding | File | Fix Applied |
|---|---------|------|-------------|
| 1 | HIGH — uninitvar `t` (cppcheck) | `sensor.c:70` | Added `= {0}` to array declaration; prevents UB if `n == 0` at call site |
| 2 | HIGH — implicit widening of multiplication (clang-tidy) | `ext_flash_storage.c:511` | Added explicit cast `(uint32_t)(s * 4096U)` to make widening intent visible to static analysis on 64-bit host |
| 3 | HIGH — unused parameter `data` (clang-tidy) | `ms5837.c:280` | Added `(void)data;` at top of `ms5837_init()`; parameter retained for API symmetry |
| 4 | MEDIUM — unreadVariable `gamma_index` (cppcheck) | `main.c:656` | **Bug fix**: `backlight_gamma_lut[10]` was hardcoded — brightness setting was being silently ignored. Changed to `backlight_gamma_lut[gamma_index]` so LCD brightness now respects user setting |
| 5 | MEDIUM — redundantAssignment `P` (cppcheck) | `ms5837.c:212` | Removed the first (uncorrected) P calculation; P is now computed only once with corrected OFF/SENS after second-order temperature compensation |
| 6 | MEDIUM — variableScope `hal_status` (cppcheck) | `ms5837.c:74` | Moved `HAL_StatusTypeDef hal_status` declaration from function scope into the for-loop body where it is first assigned |
| 7 | MEDIUM — shadowVariable `sensor_data` (cppcheck) | `screen_sensor_data.c:110` | Removed redundant `extern volatile sensor_data_t sensor_data;` local declaration; `sensor.h` (already included) provides the file-scope extern |

### Suppressed (with justification)

| # | Finding | File | Justification |
|---|---------|------|---------------|
| 8 | MEDIUM — constParam `htim` | `main.c:252` | HAL weak-symbol override — signature must match HAL exactly |
| 9 | MEDIUM — funcArgNamesDifferent `htim_base` | `hal_msp.c:802` | CubeMX-generated naming inconsistency |
| 10 | MEDIUM — constParam `hi2c` | `hal_msp.c:431` | CubeMX-generated HAL MSP hook |
| 11 | MEDIUM — knownConditionTrueFalse `next_seq==0U` | `ext_flash_storage.c:340` | Intentional UINT32_MAX wrap-around defensive guard |
| 12 | MEDIUM — knownConditionTrueFalse `num_sensors>3` | `screen_main.c:649` | Compile-time constant; dead code documents the 3-sensor constraint |
| 13 | MEDIUM — constVariable `param_madctl` | `st7789v.c:42` | HAL_SPI_Transmit requires non-const `uint8_t*` |
| 14 | MEDIUM — constParam `buf` | `w25q128.c:20` | HAL_SPI_Transmit requires non-const `uint8_t*` |
| 15 | MEDIUM — constVariable `tx` | `w25q128.c:111` | HAL_SPI_TransmitReceive requires non-const `uint8_t* pTxData` |
| 16–19 | MEDIUM — memcpy buffer (×4) | `ext_flash_storage.c:172,199,244,273` | Both src/dst are compile-time-sized buffers; sizes always match |
| 20–27 | MEDIUM — char array buffer (×8) | screen_calibration, screen_main, screen_sensor_data, screen_settings | All use `snprintf(buf, sizeof(buf), ...)` — writes bounded by declaration size |
| 28 | MEDIUM — complexity HAL_ADC_MspInit CC=17 | `hal_msp.c:115` | CubeMX-generated; not modifiable |

### Deferred (needs future refactoring task)

| # | Finding | File | Reason |
|---|---------|------|--------|
| 29 | MEDIUM — complexity `main` CC=23 | `main.c:547` | main() contains CubeMX init sections; refactoring USER CODE portions is a separate task |
| 30 | MEDIUM — `build_screen_content` length 228 lines | `screen_calibration.c:56` | Needs dedicated UI refactoring sprint |
| 31 | MEDIUM — `screen_calibration_update` CC=22 | `screen_calibration.c:513` | Needs dedicated UI refactoring sprint |
| 32 | MEDIUM — `screen_calibration_on_button` CC=21 | `screen_calibration.c:634` | Needs dedicated UI refactoring sprint |
| 33 | MEDIUM — `build_screen_content` length 309 lines | `screen_main.c:87` | Needs dedicated UI refactoring sprint |
| 34 | MEDIUM — `screen_main_update` CC=46 | `screen_main.c:519` | Highest priority refactor — CC=46 is the largest complexity risk in the codebase |
| 35 | MEDIUM — `ext_cal_iterate` CC=16 | `ext_flash_storage.c:535` | Borderline; can be addressed when ext_flash_storage is next touched |
| 36 | MEDIUM — `sensor_data_update` CC=23 | `sensor.c:182` | Borderline; can be addressed when sensor processing is next touched |
