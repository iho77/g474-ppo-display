# STM32G474 PPO Display Firmware

Firmware for the PPO (Partial Pressure of Oxygen) display unit based on the **STM32G474CC** microcontroller.

## Hardware

- MCU: STM32G474CCTx (Cortex-M4, 170 MHz, 256 KB Flash, 128 KB RAM)
- Display: SPI TFT driven via DMA
- External SPI flash for calibration/log storage
- Custom PCB (schematics in `Schematics/`)
- Enclosure model in `Housing/`

## Software Stack

| Layer | Description |
|-------|-------------|
| STM32CubeMX / HAL | Peripheral init, DMA, IRQ handlers |
| [LVGL v8.3.11](https://lvgl.io) | GUI framework (submodule) |
| Application | Sensor reading, calibration, screens, flash storage |

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

## License

Application code: MIT.
LVGL: MIT (see `STM32CubeIDE/Drivers/lvgl/LICENCE.txt`).
ST HAL / CMSIS: BSD-3 / Apache-2.0 (see respective `LICENSE.txt` files).
