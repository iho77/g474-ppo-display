Shield: [![CC BY-NC 4.0][cc-by-nc-shield]][cc-by-nc]

This work is licensed under a
[Creative Commons Attribution-NonCommercial 4.0 International License][cc-by-nc].

[![CC BY-NC 4.0][cc-by-nc-image]][cc-by-nc]

[cc-by-nc]: https://creativecommons.org/licenses/by-nc/4.0/
[cc-by-nc-image]: https://licensebuttons.net/l/by-nc/4.0/88x31.png
[cc-by-nc-shield]: https://img.shields.io/badge/License-CC%20BY--NC%204.0-lightgrey.svg

---

## DISCLAIMER — IMPORTANT, READ BEFORE USE

**THIS DEVICE IS A HOBBYIST PROTOTYPE AND EXPERIMENTAL RESEARCH TOOL. IT IS NOT A CERTIFIED DIVING INSTRUMENT, MEDICAL DEVICE, OR SAFETY EQUIPMENT OF ANY KIND.**

This firmware and associated hardware design are provided strictly for educational, experimental, and non-commercial purposes. The device has not been tested, validated, or certified to any safety standard (including but not limited to EN 13949, CE, FCC, or any diving equipment directive). Sensor readings, alarm thresholds, and all computed values may be inaccurate, delayed, or absent without warning.

**DO NOT use this device as a primary or backup oxygen monitoring system in any diving, life support, hyperbaric, medical, or other safety-critical application.** Reliance on this device in any situation involving risk to life or health is done entirely at the user's own risk.

TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THE AUTHORS AND CONTRIBUTORS DISCLAIM ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING DEATH, PERSONAL INJURY, OR PROPERTY DAMAGE) ARISING FROM THE USE OF OR INABILITY TO USE THIS DEVICE OR FIRMWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**Use of this project implies full acceptance of these terms.**

---

# STM32G474 PPO Display Firmware

Firmware for a DIY PPO2 (Partial Pressure of Oxygen) display unit based on the **STM32G474CC** microcontroller. Designed for rebreather divers who want to build and understand their own monitoring electronics.

## Features

### PPO2 Monitoring
- Reads up to three galvanic oxygen cells simultaneously via dedicated ADC channels with OPAMPs
- Displays PPO2 in bar (e.g., 1.30) for each sensor on a colour TFT screen
- Two configurable setpoints (SP1 / SP2) with quick in-dive switching via button

### Warning System
- **DRIFT** (yellow): sensor has deviated more than the configured threshold from the active setpoint
- **NEAR LIMIT** (red border): PPO2 is approaching the configured minimum or maximum safe limit
- **CRITICAL** (red background): PPO2 has crossed the minimum or maximum limit — immediate action required
- Warnings are shown per-sensor with colour-coded borders, panel backgrounds, and text
- Haptic alerts via vibration motor with distinct patterns per severity:
  - Drift → one short pulse per second
  - Near limit → two medium pulses per second
  - Critical → three rapid pulses per second
- Press the menu button to silence the vibration while the visual warning remains on screen; the vibration will resume only if the warning level changes

### Sensor Drift Tracking
- Continuously monitors the rate of change of each sensor's PPO2 over a 3-minute window
- Drift rate (mbar/min) is displayed in real time to catch failing or degrading cells early

### Depth and Temperature
- Pressure sensor (MS5837) measures ambient pressure for depth calculation
- Depth displayed in metres; temperature available in raw sensor data screen

### Two-Point Sensor Calibration
- Calibrate any sensor against two known PPO2 reference points
- Calibration coefficients stored persistently in external SPI flash
- Sensor diagnostic screen shows sensitivity history to identify cell degradation over time

### Settings
- Configurable thresholds: minimum PPO, maximum PPO, warning distance, sensor divergence limit
- Two independent setpoints with configurable values
- Display brightness control with gamma correction
- All settings survive power cycles via wear-levelled external flash storage

### Dive Log
- Session data viewable on device

### Power Management
- Battery voltage and percentage displayed at all times
- Low battery warning
- Auto power-off after configurable idle period

---

## Hardware

- MCU: STM32G474CCTx (Cortex-M4, 170 MHz, 256 KB Flash, 128 KB RAM)
- Display: SPI TFT driven via DMA
- Pressure sensor: MS5837 via I2C
- External SPI flash for calibration/log storage (W25Q128)
- Vibration motor on PA0 (TIM5 CH1 PWM)
- Custom PCB (schematics in `Schematics/`)
- Enclosure model in `Housing/`

---

## Software Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Application                          │
│                                                             │
│  screen_manager ──► screen_main / screen_settings /        │
│                      screen_calibration / screen_menu / … │
│       │                         │                           │
│  button (EXTI)            warning ◄──── sensor              │
│                             │               │               │
│                          vibro FSM      drift_tracker       │
│                         (TIM5 PWM)    (linear regression)   │
│                                           │                 │
│  settings_storage ◄──► ext_flash_storage  │                 │
│  (wear-levelled W25Q128 via SPI DMA)      │                 │
│                                           │                 │
│  pressure_sensor ◄──── ms5837 (I2C IT)   │                 │
│                                           │                 │
│  sensor_diagnostics ◄─────────────────────┘                 │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│                    HAL / CubeMX Layer                       │
│  TIM6 1kHz scheduler │ ADC1-4 DMA │ SPI1/2 DMA │ I2C1 IT  │
│  TIM2 backlight PWM  │ TIM5 vibro │ EXTI buttons           │
└─────────────────────────────────────────────────────────────┘
```

### Major Modules

| Module | File(s) | Responsibility |
|--------|---------|----------------|
| Screen manager | `screen_manager.c/.h` | Screen lifecycle (create → load → delete), button routing |
| Warning | `warning.c/.h` | PPO2 threshold checks, visual styles, vibro state machine |
| Sensor | `sensor.c/.h` | ADC → microvolts → PPO2 conversion, validity tracking |
| Drift tracker | `drift_tracker.c/.h` | 3-minute linear regression on PPO2 samples (10 s interval) |
| Sensor diagnostics | `sensor_diagnostics.c/.h` | Calibration history analysis, sensitivity degradation detection |
| Pressure sensor | `pressure_sensor.c/.h` + `ms5837.c/.h` | Depth/temperature from MS5837 via I2C interrupt-driven FSM |
| Settings storage | `settings_storage.c/.h` | Read/write 7 user parameters to external flash |
| External flash | `ext_flash_storage.c/.h` + `w25q128.c/.h` | Wear-levelled page storage on W25Q128 SPI NOR flash |
| Button | `button.c/.h` | EXTI debounce, event queue for BTN_M / BTN_S |
| Display driver | `st7789v.c/.h` | SPI DMA flush, LVGL integration |

### Scheduler

A 1 kHz hardware timer (TIM6) drives four software tick rates:

| Rate | Handler | Work done |
|------|---------|-----------|
| 5 ms | `g_flag_5ms` | LVGL render, vibro motor FSM tick |
| 20 ms | `g_flag_20ms` | Button event dispatch |
| 1 Hz | `g_flag_1hz` | Screen update, PPO2 warnings, battery check |
| 10 s | `g_flag_10s` | Drift tracker sample (3 sensors) |

---

## Repository Layout

```
Core/                          # CubeMX-generated core (Inc/, Src/)
STM32CubeIDE/
  Application/User/Core/       # Application source code
  Drivers/
    lvgl/                      # LVGL v8.3.11 (git submodule)
    lv_conf.h                  # LVGL configuration
  STM32G474CCTX_FLASH.ld       # Linker script (256 KB single-bank)
Drivers/                       # ST HAL / CMSIS (excluded from repo)
Schematics/                    # EasyEDA schematic exports
Housing/                       # FreeCAD enclosure model
stm32g474_PPO_Display.ioc      # STM32CubeMX project
```

> `Drivers/` (CMSIS + STM32G4xx HAL) is excluded — regenerate via STM32CubeMX.

---

## Building

Requires **STM32CubeIDE 1.19+**.

1. Clone with submodules:
   ```bash
   git clone --recurse-submodules https://github.com/iho77/g474-ppo-display.git
   ```
2. Open the workspace in STM32CubeIDE (`STM32CubeIDE/` folder).
3. Run **Project → Build All** (first build regenerates `subdir.mk` and `objects.list`).
4. After the first IDE build, command-line builds also work:
   ```bash
   cd STM32CubeIDE/Debug
   make -j4 all
   arm-none-eabi-size stm32g474_PPO_Display.elf
   ```

### Restoring excluded drivers

If you cloned fresh and `Drivers/` is empty, open the `.ioc` in STM32CubeMX and click **Generate Code** to restore CMSIS and HAL sources.

## Flash Memory Note

The G474CC ships with **DBANK=1** (dual 128 KB banks). The linker script uses a single 256 KB bank layout — ensure Option Bytes are set to `DBANK=0` before flashing.

---

## License

Application code: MIT.
LVGL: MIT (see `STM32CubeIDE/Drivers/lvgl/LICENCE.txt`).
ST HAL / CMSIS: BSD-3 / Apache-2.0 (see respective `LICENSE.txt` files).
