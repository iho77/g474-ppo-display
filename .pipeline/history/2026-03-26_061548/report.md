# Static Analysis Report — PPO Display

**Run timestamp:** 2026-03-26_061548
**MCU:** STM32G431KB

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 0 |
| HIGH | 12 |
| MEDIUM | 18 |
| LOW | 0 |
| **Total** | **30** |

## Findings by Tool

- **clang-tidy**: 12 findings
- **flawfinder**: 7 findings
- **lizard**: 11 findings

## Hotspot Files (most findings)

- **STM32CubeIDE/Application/User/Core/screens/screen_calibration.c**: 4 findings
- **STM32CubeIDE/Application/User/Core/dive_log.c**: 3 findings
- **STM32CubeIDE/Application/User/Core/sensor.c**: 3 findings
- **Core/Src/main.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/ext_flash_storage.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_main.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_settings.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/sensor_diagnostics.c**: 1 findings

## Detailed Findings

### HIGH (12)

**HIGH-1** | `Core/Src/main.c:766` | `clang-tidy` | `bugprone-narrowing-conversions`
> narrowing conversion from 'int' to signed type 'int8_t' (aka 'signed char') is implementation-defined

**HIGH-2** | `STM32CubeIDE/Application/User/Core/dive_log.c:109` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

**HIGH-3** | `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:146` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

**HIGH-4** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:518` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-5** | `STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c:133` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-6** | `STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c:265` | `clang-tidy` | `bugprone-narrowing-conversions`
> narrowing conversion from 'int' to signed type 'lv_coord_t' (aka 'short') is implementation-defined

**HIGH-7** | `STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:114` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-8** | `STM32CubeIDE/Application/User/Core/screens/screen_settings.c:95` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-9** | `STM32CubeIDE/Application/User/Core/sensor.c:205` | `clang-tidy` | `bugprone-implicit-widening-of-multiplication-result`
> performing an implicit widening conversion to type 'unsigned long' of a multiplication performed in type 'int'

**HIGH-10** | `STM32CubeIDE/Application/User/Core/sensor.c:206` | `clang-tidy` | `bugprone-narrowing-conversions`
> narrowing conversion from 'unsigned int' to signed type 'int32_t' (aka 'int') is implementation-defined

**HIGH-11** | `STM32CubeIDE/Application/User/Core/sensor_diagnostics.c:192` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

**HIGH-12** | `STM32CubeIDE/Application/User/Core/w25q128.c:216` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

### MEDIUM (18)

**MEDIUM-1** | `STM32CubeIDE/Application/User/Core/dive_log.c:133:5` | `flawfinder` | `buffer`
> memcpy:Does not check for buffer overflows when copying to destination (CWE-120).  Make sure destination can always hold the source data.
> *Risk level: 2/5*

**MEDIUM-2** | `STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c:125:12` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-3** | `STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c:158:12` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-4** | `STM32CubeIDE/Application/User/Core/screens/screen_dive_log.c:159:12` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-5** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:761:9` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-6** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:780:9` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-7** | `STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:111:12` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-8** | `Core/Src/main.c:517` | `lizard` | `complexity(warning: main)`
> Cyclomatic complexity 27 exceeds threshold 15
> *Function: warning: main*

**MEDIUM-9** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:56` | `lizard` | `complexity(warning: build_screen_content)`
> Function length 228 lines exceeds threshold 75
> *Function: warning: build_screen_content*

**MEDIUM-10** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:513` | `lizard` | `complexity(warning: screen_calibration_update)`
> Cyclomatic complexity 22 exceeds threshold 15
> *Function: warning: screen_calibration_update*

**MEDIUM-11** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:634` | `lizard` | `complexity(warning: screen_calibration_on_button)`
> Cyclomatic complexity 21 exceeds threshold 15
> *Function: warning: screen_calibration_on_button*

**MEDIUM-12** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:87` | `lizard` | `complexity(warning: build_screen_content)`
> Function length 309 lines exceeds threshold 75
> *Function: warning: build_screen_content*

**MEDIUM-13** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:519` | `lizard` | `complexity(warning: screen_main_update)`
> Cyclomatic complexity 46 exceeds threshold 15
> *Function: warning: screen_main_update*

**MEDIUM-14** | `STM32CubeIDE/Application/User/Core/dive_log.c:291` | `lizard` | `complexity(warning: dive_log_tick_1hz)`
> Cyclomatic complexity 36 exceeds threshold 15
> *Function: warning: dive_log_tick_1hz*

**MEDIUM-15** | `STM32CubeIDE/Application/User/Core/dive_log.c:401` | `lizard` | `complexity(warning: dive_log_iterate)`
> Cyclomatic complexity 19 exceeds threshold 15
> *Function: warning: dive_log_iterate*

**MEDIUM-16** | `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:535` | `lizard` | `complexity(warning: ext_cal_iterate)`
> Cyclomatic complexity 16 exceeds threshold 15
> *Function: warning: ext_cal_iterate*

**MEDIUM-17** | `STM32CubeIDE/Application/User/Core/screen_manager.c:94` | `lizard` | `complexity(warning: screen_manager_handle_button)`
> Cyclomatic complexity 17 exceeds threshold 15
> *Function: warning: screen_manager_handle_button*

**MEDIUM-18** | `STM32CubeIDE/Application/User/Core/sensor.c:182` | `lizard` | `complexity(warning: sensor_data_update)`
> Cyclomatic complexity 23 exceeds threshold 15
> *Function: warning: sensor_data_update*
