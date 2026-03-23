# Suppressed Findings

<!-- Add findings here that are intentionally accepted -->
<!-- Each must include a fingerprint comment and justification -->

---

## Suppressed 1

<!-- fingerprint: clang-tidy|Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g4xx.h|clang-diagnostic-error -->
- **Severity:** CRITICAL
- **Tool:** clang-tidy
- **Check:** `clang-diagnostic-error`
- **Location:** `Drivers/CMSIS/Device/ST/STM32G4xx/Include/stm32g4xx.h:111`
- **Message:** 'stm32g431xx.h' file not found
- **Justification:** clang-tidy misconfiguration — this project targets STM32G474, not G431. clang-tidy is resolving to the wrong device header. The firmware builds correctly against the proper target. Fix by updating the clang-tidy include path config to define `STM32G474xx`.

---

## Suppressed 2

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/syscalls.c|clang-diagnostic-error -->
- **Severity:** CRITICAL
- **Tool:** clang-tidy
- **Check:** `clang-diagnostic-error`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:102`
- **Message:** use of undeclared identifier 'S_IFCHR'
- **Justification:** Cascading false positive from Suppressed 1 (wrong device header breaks the POSIX include chain). `S_IFCHR` is defined in `<sys/stat.h>` which depends on the correct STM32 device header being resolved. Not a real firmware bug.

---

## Suppressed 3

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/sysmem.c|clang-diagnostic-error -->
- **Severity:** CRITICAL
- **Tool:** clang-tidy
- **Check:** `clang-diagnostic-error`
- **Location:** `STM32CubeIDE/Application/User/Core/sysmem.c:30`
- **Message:** use of undeclared identifier 'NULL'
- **Justification:** Cascading false positive from Suppressed 1 (wrong device header breaks `<stddef.h>` resolution). `NULL` is correctly defined in the actual build environment. Not a real firmware bug.

---

## Suppressed 4

<!-- fingerprint: clang-tidy|Core/Src/stm32g4xx_hal_msp.c|bugprone-branch-clone -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `bugprone-branch-clone`
- **Location:** `Core/Src/stm32g4xx_hal_msp.c:324`
- **Message:** repeated branch body in conditional chain
- **Justification:** CubeMX-generated file. Pattern is intentional — ST generates symmetric MSP init/deinit branches for multiple peripheral instances. Not user-editable without risking regeneration conflicts.

---

## Suppressed 5

<!-- fingerprint: clang-tidy|Core/Src/system_stm32g4xx.c|bugprone-branch-clone -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `bugprone-branch-clone`
- **Location:** `Core/Src/system_stm32g4xx.c:251`
- **Message:** if with identical then and else branches
- **Justification:** CubeMX-generated file. ST's clock configuration code uses symmetric if/else for conditional compilation placeholders. Not user-editable.

---

## Suppressed 6

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/screens/screen_main.c|clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:497`
- **Message:** Call to function 'snprintf' is insecure as it does not provide security checks introduced in the C11 standard.
- **Justification:** False positive on embedded target. `snprintf_s` is not available in ARM newlib. `snprintf` with an explicit size argument IS the bounds-safe function on this platform. No user input involved — format strings are compile-time constants.

---

## Suppressed 7

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/syscalls.c|bugprone-reserved-identifier -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `bugprone-reserved-identifier`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:35`
- **Message:** declaration uses identifier '__io_putchar', which is a reserved identifier
- **Justification:** ST-generated syscalls boilerplate. `__io_putchar` is the standard ST weak symbol for retargeting `printf` to UART. This is the intended pattern for this toolchain.

---

## Suppressed 8

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/syscalls.c|bugprone-narrowing-conversions -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `bugprone-narrowing-conversions`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:74`
- **Message:** narrowing conversion from 'int' to signed type 'char' is implementation-defined
- **Justification:** ST-generated syscalls boilerplate. Standard newlib retargeting pattern; value is always in `[0, 255]` range by POSIX contract.

---

## Suppressed 9

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/syscalls.c|readability-non-const-parameter -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `readability-non-const-parameter`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:120`
- **Message:** pointer parameter 'path' can be pointer to const
- **Justification:** ST-generated syscalls boilerplate. Signature must match the POSIX `_stat` declaration which does not use `const`.

---

## Suppressed 10

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/sysmem.c|bugprone-reserved-identifier -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `bugprone-reserved-identifier`
- **Location:** `STM32CubeIDE/Application/User/Core/sysmem.c:30`
- **Message:** declaration uses identifier '__sbrk_heap_end', which is a reserved identifier
- **Justification:** ST-generated sysmem boilerplate. `__sbrk_heap_end` is the standard ST/newlib heap tracking variable for the `_sbrk` syscall implementation.

---

## Suppressed 11

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/sysmem.c|clang-diagnostic-pointer-to-int-cast -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `clang-diagnostic-pointer-to-int-cast`
- **Location:** `STM32CubeIDE/Application/User/Core/sysmem.c:58`
- **Message:** cast to smaller integer type 'uint32_t' from 'uint8_t *'
- **Justification:** ST-generated sysmem boilerplate. On Cortex-M4 (32-bit), pointer and `uint32_t` are the same size. This cast is required for the `_sbrk` heap boundary arithmetic and is standard practice in ARM newlib ports.

---

## Suppressed 12

<!-- fingerprint: clang-tidy|STM32CubeIDE/Application/User/Core/sysmem.c|clang-diagnostic-int-to-pointer-cast -->
- **Severity:** HIGH
- **Tool:** clang-tidy
- **Check:** `clang-diagnostic-int-to-pointer-cast`
- **Location:** `STM32CubeIDE/Application/User/Core/sysmem.c:59`
- **Message:** cast to 'uint8_t *' from smaller integer type 'uint32_t'
- **Justification:** ST-generated sysmem boilerplate. Paired with Suppressed 11 — same `_sbrk` heap arithmetic pattern. Valid on 32-bit Cortex-M4.

---

## Suppressed 13

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/syscalls.c|constParameterPointer -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constParameterPointer`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:80`
- **Message:** Parameter 'ptr' can be declared as pointer to const
- **Justification:** ST-generated syscalls boilerplate. Signature must match the POSIX `_write` syscall declaration.

---

## Suppressed 14

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/syscalls.c:39|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/syscalls.c:39:1`
- **Message:** char:Statically-sized arrays can be improperly restricted
- **Justification:** ST-generated syscalls boilerplate. Fixed-size UART retargeting buffer; size is appropriate for the use case and not driven by external input.

---

## Suppressed 15

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/screens/screen_calibration.c|knownConditionTrueFalse -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `knownConditionTrueFalse`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:66`
- **Message:** Same expression on both sides of '|' because 'LV_PART_MAIN' and 'LV_STATE_DEFAULT' represent the same value.
- **Justification:** Idiomatic LVGL v8 style. `LV_PART_MAIN | LV_STATE_DEFAULT` is the canonical selector expression used throughout LVGL examples and documentation. Both constants are 0 by design; the expression evaluates to 0 which is the correct default selector. Not a logic error.

---

## Suppressed 16

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/screens/screen_menu.c|knownConditionTrueFalse -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `knownConditionTrueFalse`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_menu.c:58`
- **Message:** Same expression on both sides of '|' because 'LV_PART_MAIN' and 'LV_STATE_DEFAULT' represent the same value.
- **Justification:** Same as Suppressed 15 — idiomatic LVGL v8 style selector expression.

---

## Suppressed 17

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/warning.c|knownConditionTrueFalse -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `knownConditionTrueFalse`
- **Location:** `STM32CubeIDE/Application/User/Core/warning.c:301`
- **Message:** Same expression on both sides of '|' because 'LV_PART_MAIN' and 'LV_STATE_DEFAULT' represent the same value.
- **Justification:** Same as Suppressed 15 — idiomatic LVGL v8 style selector expression.

---

## Suppressed 18

<!-- fingerprint: cppcheck|Core/Src/main.c|constParameterPointer -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constParameterPointer`
- **Location:** `Core/Src/main.c:252`
- **Message:** Parameter 'htim' can be declared as pointer to const
- **Justification:** `HAL_TIM_PeriodElapsedCallback` is a HAL weak-symbol override. The signature must exactly match the HAL declaration (`TIM_HandleTypeDef *htim`). Adding `const` would create a different function prototype that would NOT override the weak symbol, silently breaking the callback.

---

## Suppressed 19

<!-- fingerprint: cppcheck|Core/Src/stm32g4xx_hal_msp.c|funcArgNamesDifferent -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `funcArgNamesDifferent`
- **Location:** `Core/Src/stm32g4xx_hal_msp.c:802`
- **Message:** Function 'HAL_TIM_Base_MspInit' argument 1 names different: declaration 'htim' definition 'htim_base'.
- **Justification:** CubeMX-generated code. ST uses `htim_base` as the local parameter name in the MSP init definition while the HAL header uses `htim`. This is an ST naming inconsistency in generated code, not a bug.

---

## Suppressed 20

<!-- fingerprint: cppcheck|Core/Src/stm32g4xx_hal_msp.c|constParameterPointer -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constParameterPointer`
- **Location:** `Core/Src/stm32g4xx_hal_msp.c:431`
- **Message:** Parameter 'hi2c' can be declared as pointer to const
- **Justification:** CubeMX-generated HAL MSP hook. Signature must match the HAL `__weak` declaration exactly. Not modifiable without risking CubeMX regeneration conflicts.

---

## Suppressed 21

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/ext_flash_storage.c|knownConditionTrueFalse -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `knownConditionTrueFalse`
- **Location:** `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:340`
- **Message:** Condition 'next_seq==0U' is always false
- **Justification:** Intentional defensive guard against UINT32_MAX sequence counter wrap-around: `if (next_seq == 0U) next_seq = 1U`. cppcheck proves it can't happen given static analysis paths, but the guard is retained as defensive coding for long-running deployments where the sequence counter could theoretically reach UINT32_MAX after ~4 billion writes.

---

## Suppressed 22

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/screens/screen_main.c|knownConditionTrueFalse -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `knownConditionTrueFalse`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:649`
- **Message:** Condition 'num_sensors>3' is always false
- **Justification:** `SENSOR_COUNT` is a compile-time enum constant (=5). `num_sensors = SENSOR_COUNT - 2 = 3` always. The clamp `if (num_sensors > 3) num_sensors = 3` is dead code but documents the architectural constraint (exactly 3 O2 sensors). Retained for future maintainability when SENSOR_COUNT changes.

---

## Suppressed 23

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/st7789v.c|constVariable -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constVariable`
- **Location:** `STM32CubeIDE/Application/User/Core/st7789v.c:42`
- **Message:** Variable 'param_madctl' can be declared as const array
- **Justification:** `param_madctl` is passed directly to `HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, ...)` which requires a non-const `uint8_t*`. Adding `const` would require an unsafe cast to satisfy the HAL API.

---

## Suppressed 24

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/w25q128.c|constParameterPointer -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constParameterPointer`
- **Location:** `STM32CubeIDE/Application/User/Core/w25q128.c:20`
- **Message:** Parameter 'buf' can be declared as pointer to const
- **Justification:** `spi_tx()` passes `buf` directly to `HAL_SPI_Transmit(SPI_HandleTypeDef*, uint8_t*, ...)`. The HAL API requires non-const. Adding `const` to the wrapper parameter would require a discarding cast.

---

## Suppressed 25

<!-- fingerprint: cppcheck|STM32CubeIDE/Application/User/Core/w25q128.c|constVariable -->
- **Severity:** MEDIUM
- **Tool:** cppcheck
- **Check:** `constVariable`
- **Location:** `STM32CubeIDE/Application/User/Core/w25q128.c:111`
- **Message:** Variable 'tx' can be declared as const array
- **Justification:** `tx` is passed to `HAL_SPI_TransmitReceive(SPI_HandleTypeDef*, uint8_t* pTxData, ...)`. The HAL API requires non-const `pTxData`. Declaring `tx` as `const` would require an unsafe cast.

---

## Suppressed 26

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/ext_flash_storage.c:172|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:172:5`
- **Message:** memcpy — does not check for buffer overflows (CWE-120)
- **Justification:** `memcpy(tmp, buf, EXT_SETTINGS_ENTRY_SIZE)` — both `tmp` and `buf` are declared as `EXT_SETTINGS_ENTRY_SIZE`-byte (28-byte) buffers. Size is a compile-time constant; source and destination are always correctly sized.

---

## Suppressed 27

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/ext_flash_storage.c:199|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:199:5`
- **Message:** memcpy — does not check for buffer overflows (CWE-120)
- **Justification:** Same as Suppressed 26 — `memcpy(tmp, out, EXT_SETTINGS_ENTRY_SIZE)` with matching compile-time-sized buffers.

---

## Suppressed 28

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/ext_flash_storage.c:244|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:244:5`
- **Message:** memcpy — does not check for buffer overflows (CWE-120)
- **Justification:** `memcpy(tmp, buf, EXT_CAL_ENTRY_SIZE)` — both buffers are `EXT_CAL_ENTRY_SIZE`-byte (48-byte) compile-time-sized allocations. No overflow possible.

---

## Suppressed 29

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/ext_flash_storage.c:273|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/ext_flash_storage.c:273:5`
- **Message:** memcpy — does not check for buffer overflows (CWE-120)
- **Justification:** Same as Suppressed 28 — `memcpy(tmp, buf, EXT_CAL_ENTRY_SIZE)` with matching compile-time-sized buffers.

---

## Suppressed 30

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:514|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:514:5`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Static `char` buffer used with `snprintf(buf, sizeof(buf), ...)`. The `sizeof(buf)` argument bounds all writes to the declared array size. No external input reaches this buffer without truncation.

---

## Suppressed 31

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:635|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_calibration.c:635:5`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 32

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_main.c:496|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:496:12`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 33

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_main.c:526|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:526:5`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 34

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_main.c:768|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:768:9`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 35

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_main.c:787|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_main.c:787:9`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 36

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:112|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_sensor_data.c:112:12`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 37

<!-- fingerprint: flawfinder|STM32CubeIDE/Application/User/Core/screens/screen_settings.c:92|buffer -->
- **Severity:** MEDIUM
- **Tool:** flawfinder
- **Check:** `buffer`
- **Location:** `STM32CubeIDE/Application/User/Core/screens/screen_settings.c:92:12`
- **Message:** char — statically-sized arrays can be improperly restricted (CWE-119/120)
- **Justification:** Same as Suppressed 30 — static `char` buffer with `snprintf(buf, sizeof(buf), ...)`.

---

## Suppressed 38

<!-- fingerprint: lizard|Core/Src/stm32g4xx_hal_msp.c|complexity(warning: HAL_ADC_MspInit) -->
- **Severity:** MEDIUM
- **Tool:** lizard
- **Check:** `complexity(warning: HAL_ADC_MspInit)`
- **Location:** `Core/Src/stm32g4xx_hal_msp.c:115`
- **Message:** Cyclomatic complexity 17 exceeds threshold 15
- **Justification:** CubeMX-generated `HAL_ADC_MspInit`. Complexity reflects the number of ADC channels/DMA streams being configured by ST's code generator. Not user-editable.
