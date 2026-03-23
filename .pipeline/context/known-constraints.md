# Known Constraints & Intentional Deviations

## Hardware Quirks — STM32G474CCTx
- 128 KB SRAM, 256 KB Flash (single-bank) — **DBANK=0 must be set in Options Bytes**;
  factory default DBANK=1 splits flash into two 128 KB banks and breaks the 256 KB linker script
- No external RAM — LVGL 20-line partial framebuffer (~15 KB) fits within internal 128 KB SRAM
- OPAMP1/OPAMP2/OPAMP3 used for 16× signal conditioning of three O2 sensor cells;
  outputs routed internally to ADC1/ADC2/ADC3 channels respectively
- Internal Vrefint (via ADC4) used for ADC calibration reference
- I2C1 has **no DMA stream configured** — MS5837 pressure sensor uses IT (interrupt) mode only;
  never call `HAL_I2C_Master_Receive_DMA()` on I2C1

## External Flash (W25Q128 via SPI1)
- All persistent data lives on the W25Q128, not internal flash
- Settings: ping-pong wear-levelling across sector 0 and sector 1 (4 KB each), 28-byte entries, CRC32
- Calibration: append-only log in sectors 2–17 (64 KB), 48-byte entries, up to 1280 records
- SPI1 is dedicated to the W25Q128; do not share it with other peripherals

## Intentional Patterns
- **CubeMX-generated code style**: HAL MSP, IT handler, and main.c init sections follow
  STM32CubeMX conventions with `USER CODE BEGIN/END` markers. Analysis should focus on
  USER CODE sections, not generated boilerplate. Never modify code outside USER CODE blocks.
- **LVGL memory management**: LVGL v8.3.11 uses its own heap allocator (`lv_mem`).
  Standard malloc/free warnings in LVGL code are expected and should be suppressed.
- **HAL library warnings**: Findings inside `Drivers/STM32G4xx_HAL_Driver/` are out of scope
  — we do not modify vendor code.
- **No HAL re-initialization in app code**: all peripherals are configured once by CubeMX-
  generated code; application code only calls `HAL_*_Start()` / runtime control functions.
- **Flash I/O uses uint64_t array packing** (not `__attribute__((packed))` structs) to avoid
  unaligned access faults on Cortex-M4.
- **LVGL screen lifecycle**: Create → Load → Delete (in that order) to prevent HardFault from
  deleting an active screen.
- **DMA flush completion**: `lv_disp_flush_ready()` is called in `HAL_SPI_TxCpltCallback()`,
  never inside the flush callback itself.

## Build Configuration
- Compiler: arm-none-eabi-gcc (GNU Tools for STM32, STM32CubeIDE 1.19.0)
- Standard: C11 (gnu11)
- Optimization: Debug `-O0`, Release `-Os` or `-O2`
- Linker script: `STM32G474CCTX_FLASH.ld` (256 KB single-bank)
- First build after adding new `.c` files **must** be done inside STM32CubeIDE (Project → Build All)
  to regenerate `subdir.mk` and `objects.list`; after that, command-line `make` works

## Known Limitations to Accept
- No hardware watchdog currently enabled (tracked as HIGH finding in safety audit)
- Fault handlers loop forever (no reset on hard fault) — intentional for debugger attachment
- No I2C bus recovery on lock-up (tracked as HIGH finding)
