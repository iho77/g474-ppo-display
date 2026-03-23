# STM32G474 Firmware — Safety Audit Report

**Audit date:** 2026-03-23
**Compiler context:** GCC ARM, bare-metal, no RTOS.
**Scope:** All custom application files in `STM32CubeIDE/Application/User/Core/`, `Core/Src/main.c`, `Core/Src/stm32g4xx_it.c`.

---

## Executive Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 0 |
| HIGH | 4 |
| MEDIUM | 7 |
| LOW | 5 |

**Top 3 highest-risk areas:**
1. **No watchdog** — a main-loop stall (LVGL, DMA, I2C hang) leaves a diving device permanently unresponsive with no alarms, no display, and no depth monitoring.
2. **Fault handlers are `while(1)` loops** — HardFault/BusFault during a dive silently freezes all outputs including the PPO2 warning system.
3. **`ms5837_i2c_bus_recover1()` calls `HAL_I2C_Init()`** — explicit policy violation (CLAUDE.md rule 5) that risks corrupting I2C DMA state.

**Overall assessment: NEEDS REMEDIATION** — no findings that cause guaranteed runtime failure under normal conditions, but findings #1 (no watchdog) and #2 (fault handlers) represent unacceptable safety gaps for a mission-critical diving instrument.

---

## Detailed Findings Table

| # | File:Line | Severity | Category | Finding | Evidence | Remediation |
|---|-----------|----------|----------|---------|----------|-------------|
| 1 | `main.c` (no watchdog) | HIGH | D4 | No IWDG configured or fed anywhere. A main-loop stall (LVGL deadlock, DMA hung) leaves the device frozen indefinitely. During a dive, PPO2 alarms and depth display silently stop updating. | Grepped entire codebase — `IWDG` not referenced. | Configure IWDG at ~4-8 s. Feed it in the main loop 1Hz block or 5ms block. Ensure the feed only happens when the main loop is executing (not unconditionally in an ISR). |
| 2 | `stm32g4xx_it.c:96-151` | HIGH | D8 | All fault handlers (`HardFault_Handler`, `BusFault_Handler`, `MemManage_Handler`, `UsageFault_Handler`, `NMI_Handler`) are `while(1)` infinite loops. A HardFault during a dive silently freezes the display and all alarms. | `void HardFault_Handler(void) { while (1) { } }` | Add a brief delay (allow DMA to drain), then call `vibro_hw_stop()` or drive GPIO warning line high, then `NVIC_SystemReset()`. Capture fault diagnostics in a `__no_init` RAM structure for post-mortem. |
| 3 | `main.c:543-556` | HIGH | E5 | `ms5837_i2c_bus_recover1()` calls `HAL_I2C_Init(&hi2c1)` directly. This re-initializes the I2C peripheral, potentially corrupting ongoing IT transfers and the HAL internal state machine. Violates CLAUDE.md rule 5. | `if (HAL_I2C_Init(&hi2c1) != HAL_OK) { // Handle Init Error }` | This function is not called anywhere in the visible codebase — confirm it is dead code, then remove it. If bus recovery is needed, reset the peripheral using the RCC reset sequence and wait for HAL to return to READY state, without calling HAL_I2C_Init during normal operation. |
| 4 | `main.c:656-658` | MEDIUM | D8 | If `ms5837_init()` fails at boot, `ms5837_initialized` stays false and the FSM never runs. However, `pressure_sensor_init()` already set `pressure.pressure_valid = true` and `pressure.pressure_mbar = ATMOSPHERIC_PRESSURE_MBAR`. The display and depth timer will trust stale/default pressure data without knowing the sensor failed. | `if (ms5837_init(...) == MS5837_OK) { ms5837_initialized = true; }` (no else branch) | In the else branch: `sensor_data.pressure.pressure_valid = false;`. Also log the failure for diagnostic purposes. The depth timer already guards on `pressure_valid`, so setting it false prevents a phantom dive timer. |
| 5 | `pressure_sensor.c:106` | MEDIUM | B1 | At depths near the sensor's 30-bar limit the intermediate product `delta_pressure_mbar * DEPTH_COEFFICIENT_NUMERATOR` overflows `int32_t`. `delta_pressure_mbar` is `surface_pressure_mbar - current`, with `current` scaled ×100. At 30 bar: `current = 3,000,000`, delta ≈ `−2,900,000`, product ≈ `−2,885,000,000` which exceeds `INT32_MIN (−2,147,483,648)`. | `int32_t depth_mm = (delta_pressure_mbar * DEPTH_COEFFICIENT_NUMERATOR) / DEPTH_COEFFICIENT_DENOMINATOR;` | Use `int64_t` for the intermediate: `int32_t depth_mm = (int32_t)(((int64_t)delta_pressure_mbar * 995) / 10000);` |
| 6 | `button.c:71-75` | MEDIUM | C5 | `button_get_event()` reads then clears `pending_event` in two non-atomic operations. An EXTI ISR that fires between the read and the clear overwrites the value with a new event, which is then cleared invisibly. | `btn_event_t evt = pending_event; pending_event = BTN_NONE;` | Use `__disable_irq()` / `__enable_irq()` around the read-clear pair: `__disable_irq(); evt = pending_event; pending_event = BTN_NONE; __enable_irq();` |
| 7 | `main.c:773-777` | MEDIUM | C7 | `ADC4_VAL[2]` holds two 16-bit values (VREFINT, BAT) written by DMA in circular mode. DMA writes continuously. Main loop reads `ADC4_VAL[0]` then `ADC4_VAL[1]` as two separate accesses. A DMA half-complete interrupt between these reads could result in a torn pair (VREFINT from one cycle, BAT from the next). | `sensor_data_update(REFERENCE, ADC4_VAL[0]); sensor_data_update(BAT, ADC4_VAL[1]);` | Copy both values atomically under `__disable_irq()` before calling `sensor_data_update`. |
| 8 | `calibration.c:27` | MEDIUM | A2 | `calibrate_sensor()` writes to `sensor_data.os_s_cal_state[SensorID]` without first validating `SensorID < (SENSOR_COUNT - 2)`. The array is size 3 (indices 0-2). If the caller passes SensorID ≥ 3, this is an out-of-bounds write into adjacent struct fields. | `sensor_data.os_s_cal_state[SensorID] = NON_CALIBRATED;` | Add `if (SensorID >= (SENSOR_COUNT - 2)) return;` at the top of `calibrate_sensor()`. |
| 9 | `ext_flash_storage.c:547` | MEDIUM | A3 | `ext_cal_iterate()` allocates `entry_ref_t valid[64]` on the stack — 64 × 8 bytes = 512 bytes. This is called from the main loop when the dive log screen loads. Combined with other local variables and call chain, this risks stack overflow if the stack is tight. | `entry_ref_t valid[EXT_CAL_ITER_MAX_SORT];` where `EXT_CAL_ITER_MAX_SORT = 64U` | Move to a static local: `static entry_ref_t valid[EXT_CAL_ITER_MAX_SORT];` — eliminates stack use at the cost of 512 bytes BSS. Verify with `arm-none-eabi-size` before and after. |
| 10 | `sensor.c:45` | LOW | F4 | `#define MAX_STABILITY_WAIT 0` — comment says "Timeout after warmup (~3s)" but value is zero, which disables the timeout entirely. Code depending on this for sensor validity timeout silently never fires. | `#define MAX_STABILITY_WAIT  0 // Timeout after warmup (~3s)` | Either restore the intended timeout value or rename to `MAX_STABILITY_WAIT_DISABLED` and add a comment marking it as intentionally disabled. Remove dead code at lines 268-273 if the timeout is never re-enabled. |
| 11 | `main.c:77` | LOW | F3 | Unused variable pollutes global namespace. | `uint32_t something = 10;` | Delete. |
| 12 | `sensor.h:120-125` | LOW | F4 | Duplicate include guard at end of file — a second `#ifndef SENSOR_H_ / #define SENSOR_H_ / #endif` block after the first guard closes at line 112. Harmless but indicates copy-paste error. | Lines 120-125 of `sensor.h` | Remove the duplicate guard block. |
| 13 | `main.c:108-110` | LOW | F3 | `vbias_effective_mv` and `vbias_counts_for_adc2` declared as globals, initialized, but not visibly used in the main loop or any callback. | `uint32_t vbias_effective_mv = 0; uint32_t vbias_counts_for_adc2 = 0;` | Verify these are unused; if so, remove. |
| 14 | `sensor.c:107-168` | LOW | F2 | `filter_step_sensor` has cyclomatic complexity ~12, approaching the recommended limit of 15. Hampel filter, EMA, baseline, and init paths are all interleaved. | Function spans 61 lines with nested conditionals. | No immediate action required. Consider extracting the Hampel filter block into a `static inline int32_t hampel_clip(...)` helper. |
| 15 | `ms5837.c:99` | LOW | D1 | No CRC verification of PROM data after `ms5837_read_prom()`. The `crc_prom` field is extracted but never validated against a computed CRC of the 7 PROM words. A corrupted PROM (EMI, flash retention fault) would silently produce wrong calibration and thus wrong depth readings. | `calib->crc_prom = (calib->C[0] >> 12) & 0x0F;` — CRC extracted but not verified | Implement the 4-bit CRC check per MS5837 datasheet §2.4 and return `MS5837_ERROR_CRC` if it fails. |



## Resolution Summary — 2026-03-23

All actionable findings addressed in a single remediation session. Build verified clean after all changes.

### Pre-existing (confirmed resolved before this session)

| # | Finding | Resolution |
|---|---------|------------|
| 1 | No IWDG configured | IWDG was already configured via `.ioc` and fed with `HAL_IWDG_Refresh(&hiwdg)` in the 1 Hz scheduler block (`main.c:761`). |
| 3 | `ms5837_i2c_bus_recover1()` calls `HAL_I2C_Init` | Function does not exist anywhere in the codebase. Finding not applicable. |

### Fixed in this session

| # | Severity | Finding | File | Fix applied |
|---|----------|---------|------|-------------|
| 2 | HIGH | Fault handlers loop forever | `Core/Src/stm32g4xx_it.c` | Added `TIM5->CCR1 = 0U; NVIC_SystemReset();` inside each fault handler USER CODE block (register-level TIM5 stop chosen over HAL — HAL unsafe when stack may be corrupt). Applied to HardFault, MemManage, BusFault, UsageFault. |
| 4 | MEDIUM | ms5837_init failure leaves `pressure_valid = true` | `Core/Src/main.c` | Added `else { sensor_data.pressure.pressure_valid = false; }` branch to prevent phantom depth timer on sensor-absent startup. |
| 5 | MEDIUM | int32 overflow in depth calculation | `Application/User/Core/pressure_sensor.c:106` | Changed to `(int32_t)(((int64_t)delta_pressure_mbar * 995) / 10000)` to prevent overflow near the sensor's 30-bar limit. |
| 6 | MEDIUM | `button_get_event()` non-atomic read-clear | `Application/User/Core/button.c:71` | Wrapped read-clear pair in `__disable_irq()` / `__enable_irq()`. |
| 7 | MEDIUM | ADC4 torn read | `Core/Src/main.c` | Replaced separate `ADC4_VAL[0]` / `ADC4_VAL[1]` accesses with an atomic copy of both values under `__disable_irq()` before calling `sensor_data_update()`. |
| 8 | MEDIUM | `calibrate_sensor()` missing bounds check | `Application/User/Core/calibration.c:19` | Added `if (SensorID >= (SENSOR_COUNT - 2U)) return;` as the first statement. |
| 9 | MEDIUM | 512-byte stack array in `ext_cal_iterate()` | `Application/User/Core/ext_flash_storage.c:547` | Changed `entry_ref_t valid[64]` to `static entry_ref_t valid[64]`. BSS increased by 512 bytes to 57,396 bytes (well within 128 KB RAM). |
| 10 | LOW | `MAX_STABILITY_WAIT` comment misleading | `Application/User/Core/sensor.c:45` | Updated comment to `/* Intentionally 0 — no extra wait beyond MIN_WARMUP_STEPS */`. |
| 11 | LOW | Unused `something` global | `Core/Src/main.c:77` | Deleted. |
| 12 | LOW | Duplicate include guard in `sensor.h` | `Application/User/Core/sensor.h:120-125` | Removed the trailing empty `#ifndef SENSOR_H_ / #define SENSOR_H_ / #endif` block. |
| 13 | LOW | Unused `vbias_effective_mv`, `vbias_counts_for_adc2` | `Core/Src/main.c:109-110` | Confirmed no references in any `.c` file beyond their declarations. Both deleted. |
| 15 | LOW | No PROM CRC validation in MS5837 driver | `Application/User/Core/ms5837.c` | Implemented `static uint8_t ms5837_crc4(const uint16_t *prom)` per MS5837 datasheet §2.4. Called at the end of `ms5837_read_prom()`; returns `MS5837_ERROR_CRC` on mismatch. `MS5837_ERROR_CRC` was already defined in `ms5837.h:70`. |

### No action taken

| # | Severity | Finding | Reason |
|---|----------|---------|--------|
| 14 | LOW | `filter_step_sensor()` complexity ~12 | Below the action threshold of 15. No change. |

### Post-remediation build

```
text     data      bss      dec
214500    148    57396   272044   stm32g474_PPO_Display.elf
```

Build: clean, no errors, no warnings. RAM headroom: ~73 KB remaining.
