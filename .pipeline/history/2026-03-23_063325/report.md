# Static Analysis Report — PPO Display

**Run timestamp:** 2026-03-23_063325
**MCU:** STM32G431KB

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 0 |
| HIGH | 8 |
| MEDIUM | 9 |
| LOW | 0 |
| **Total** | **17** |

## Findings by Tool

- **clang-tidy**: 8 findings
- **flawfinder**: 1 findings
- **lizard**: 8 findings

## Hotspot Files (most findings)

- **STM32CubeIDE/Application/User/Core/screens/screen_calibration.c**: 4 findings
- **STM32CubeIDE/Application/User/Core/sensor.c**: 3 findings
- **STM32CubeIDE/Application/User/Core/ext_flash_storage.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_main.c**: 2 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_settings.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/sensor_diagnostics.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/w25q128.c**: 1 findings
- **STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:111**: 1 findings
- **Core/Src/main.c**: 1 findings

## Detailed Findings

### HIGH (8)

**HIGH-1** | `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:146` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

**HIGH-2** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:518` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-3** | `STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:114` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-4** | `STM32CubeIDE/Application/User/Core/screens/screen_settings.c:95` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'snprintf_s' in case of C11

**HIGH-5** | `STM32CubeIDE/Application/User/Core/sensor.c:205` | `clang-tidy` | `bugprone-implicit-widening-of-multiplication-result`
> performing an implicit widening conversion to type 'unsigned long' of a multiplication performed in type 'int'

**HIGH-6** | `STM32CubeIDE/Application/User/Core/sensor.c:206` | `clang-tidy` | `bugprone-narrowing-conversions`
> narrowing conversion from 'unsigned int' to signed type 'int32_t' (aka 'int') is implementation-defined

**HIGH-7** | `STM32CubeIDE/Application/User/Core/sensor_diagnostics.c:192` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

**HIGH-8** | `STM32CubeIDE/Application/User/Core/w25q128.c:216` | `clang-tidy` | `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
> Call to function 'memset' is insecure as it does not provide security checks introduced in the C11 standard. Replace with analogous functions that support length arguments or provides boundary checks such as 'memset_s' in case of C11

### MEDIUM (9)

**MEDIUM-1** | `STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:111:12` | `flawfinder` | `buffer`
> char:Statically-sized arrays can be improperly restricted, leading to potential overflows or other issues (CWE-119!/CWE-120).  Perform bounds checking, use functions that limit length, or ensure that the size is larger than the maximum possible length.
> *Risk level: 2/5*

**MEDIUM-2** | `Core/Src/main.c:550` | `lizard` | `complexity(warning: main)`
> Cyclomatic complexity 23 exceeds threshold 15
> *Function: warning: main*

**MEDIUM-3** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:56` | `lizard` | `complexity(warning: build_screen_content)`
> Function length 228 lines exceeds threshold 75
> *Function: warning: build_screen_content*

**MEDIUM-4** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:513` | `lizard` | `complexity(warning: screen_calibration_update)`
> Cyclomatic complexity 22 exceeds threshold 15
> *Function: warning: screen_calibration_update*

**MEDIUM-5** | `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:634` | `lizard` | `complexity(warning: screen_calibration_on_button)`
> Cyclomatic complexity 21 exceeds threshold 15
> *Function: warning: screen_calibration_on_button*

**MEDIUM-6** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:87` | `lizard` | `complexity(warning: build_screen_content)`
> Function length 309 lines exceeds threshold 75
> *Function: warning: build_screen_content*

**MEDIUM-7** | `STM32CubeIDE/Application/User/Core/screens/screen_main.c:519` | `lizard` | `complexity(warning: screen_main_update)`
> Cyclomatic complexity 46 exceeds threshold 15
> *Function: warning: screen_main_update*

**MEDIUM-8** | `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:535` | `lizard` | `complexity(warning: ext_cal_iterate)`
> Cyclomatic complexity 16 exceeds threshold 15
> *Function: warning: ext_cal_iterate*

**MEDIUM-9** | `STM32CubeIDE/Application/User/Core/sensor.c:182` | `lizard` | `complexity(warning: sensor_data_update)`
> Cyclomatic complexity 23 exceeds threshold 15
> *Function: warning: sensor_data_update*
