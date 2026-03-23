# Firmware Architecture

## Execution Model
Bare-metal super-loop. TIM6 fires at 1 kHz and sets scheduler flags; the main loop
polls those flags and dispatches periodic work. No RTOS.

## System Clock
PLL from 16 MHz HSI -> 96 MHz system clock (PLLN=12, PLLM=1, PLLR=2).
APB1/APB2 undivided (96 MHz). Flash latency 3 cycles.

## Module Map

| Module | File(s) | Purpose | ISR? | DMA? | Shared state? | External input? |
|--------|---------|---------|------|------|---------------|-----------------|
| Init & super-loop | Core/Src/main.c | Peripheral init (CubeMX), flag dispatch, HAL callbacks | No | Yes (ADC1-4, SPI2) | Yes | No |
| Interrupt vectors | Core/Src/stm32g4xx_it.c | EXTI, DMA, ADC, I2C, SPI, TIM ISR stubs | Yes | No | Yes | Yes |
| HAL low-level init | Core/Src/stm32g4xx_hal_msp.c | GPIO, DMA, clock MSP hooks | No | No | No | No |
| sensor.c | Application/User/Core/ | ADC->PPO2: Hampel spike filter + dual-EMA, VREFINT ratio calibration, battery voltage | No | Consumes DMA bufs | Yes | Yes (ADC1-4 DMA) |
| pressure_sensor.c | Application/User/Core/ | Depth calculation from MS5837 raw ADC, salt/fresh water selection | No | No | Yes | Yes (I2C1 IT) |
| ms5837.c | Application/User/Core/ | MS5837-30BA I2C driver -- IT-driven FSM (RESET->PROM->D2->D1->READ) | Yes (I2C1 callbacks) | No | Yes | Yes (I2C1) |
| drift_tracker.c | Application/User/Core/ | 3-minute linear regression on PPO2 (10 s samples, circular buffer) | No | No | Yes | No |
| warning.c | Application/User/Core/ | Threshold checks (CRITICAL/NEAR_CRITICAL/DRIFT), vibro motor 3-level PWM FSM | No | No | Yes | No |
| calibration.c | Application/User/Core/ | Two-point sensor calibration workflow | No | No | Yes | No |
| sensor_diagnostics.c | Application/User/Core/ | Calibration history analysis, sensitivity degradation detection | No | No | Yes | No |
| settings_storage.c | Application/User/Core/ | 7-parameter settings: brightness, thresholds, setpoints. Delegates to ext_flash_storage | No | No | Yes | No |
| ext_flash_storage.c | Application/User/Core/ | W25Q128 wear-levelled settings (ping-pong) + append-only calibration log; CRC32 validation | No | No | No | No |
| w25q128.c | Application/User/Core/ | Low-level W25Q128 SPI1 driver (read/write/erase) | No | No | No | No |
| st7789v.c | Application/User/Core/ | ST7789V SPI2 DMA display driver, 20-line partial framebuffer flush | No | Yes (SPI2 DMA1 Ch1) | Yes | No |
| screen_manager.c | Application/User/Core/ | Screen lifecycle: create -> load -> delete; factory pattern | No | No | Yes | No |
| button.c | Application/User/Core/ | EXTI debounce (50 ms), press/hold detection, event queue | Yes (EXTI callback) | No | Yes | Yes (PA2, PA3) |
| screens/screen_main.c | Application/User/Core/screens/ | Primary dive display: 3x PPO2, depth, battery, drift, setpoint selector | No | No | Yes (reads sensor) | No |
| screens/screen_calibration.c | Application/User/Core/screens/ | Two-point calibration UI workflow | No | No | Yes | No |
| screens/screen_settings.c | Application/User/Core/screens/ | User preference editor | No | No | Yes | No |
| screens/screen_menu.c | Application/User/Core/screens/ | Menu navigation hub | No | No | No | No |
| screens/screen_sensor_data.c | Application/User/Core/screens/ | Raw readings, diagnostics view | No | No | Yes | No |
| screens/screen_dive_log.c | Application/User/Core/screens/ | Historical calibration/dive data viewer | No | No | No | No |
| screens/screen_startup.c | Application/User/Core/screens/ | Boot splash, sensor stabilization wait | No | No | No | No |
| screens/screen_power_off.c | Application/User/Core/screens/ | Shutdown confirmation sequence | No | No | No | No |

## Interrupt Handlers

| ISR | Source | Priority | Notes |
|-----|--------|----------|-------|
| SysTick_Handler | 1 ms tick | (HAL default) | Increments LVGL tick; calls button_tick() |
| TIM6_DAC_IRQHandler | TIM6 1 kHz | -- | Sets scheduler flags: g_flag_1hz/5ms/20ms/10s |
| EXTI2_IRQHandler | PA2 (BTN_M) | -- | Routes to button_exti_callback() |
| EXTI3_IRQHandler | PA3 (BTN_S) | -- | Routes to button_exti_callback() |
| DMA1_Channel1_IRQHandler | SPI2 TX complete | 0,0 | HAL_SPI_TxCpltCallback() -> lv_disp_flush_ready() |
| DMA1_Channel4_IRQHandler | ADC1 (SENSOR1) | 0,0 | HAL_ADC_ConvCpltCallback() -> sets g_adc1_new |
| DMA1_Channel5_IRQHandler | ADC2 (SENSOR2) | 0,0 | sets g_adc2_new |
| DMA1_Channel6_IRQHandler | ADC3 (SENSOR3) | 0,0 | sets g_adc3_new |
| DMA1_Channel7_IRQHandler | ADC4 (VREFINT+BAT) | 0,0 | sets g_adc4_new |
| ADC1_2_IRQHandler | ADC1/ADC2 shared | -- | HAL managed |
| ADC3_IRQHandler | ADC3 | -- | HAL managed |
| ADC4_IRQHandler | ADC4 | -- | HAL managed |
| I2C1_EV_IRQHandler | I2C1 event | -- | MS5837 FSM TX/RX complete callbacks |
| I2C1_ER_IRQHandler | I2C1 error | -- | MS5837 FSM -> idle |
| SPI2_IRQHandler | SPI2 general | -- | HAL managed |
| NMI_Handler / HardFault_Handler | -- | -- | Infinite loop (debugger attachment) |

## DMA Channels

| DMA Channel | Peripheral | Buffer | Circular? |
|-------------|-----------|--------|-----------|
| DMA1 Channel 1 | SPI2 TX (LCD ST7789V) | 20-line partial framebuffer | No (per-flush) |
| DMA1 Channel 4 | ADC1 (SENSOR1 via OPAMP1) | ADC1_VAL[1] uint16_t | Yes |
| DMA1 Channel 5 | ADC2 (SENSOR2 via OPAMP2) | ADC2_VAL[1] uint16_t | Yes |
| DMA1 Channel 6 | ADC3 (SENSOR3 via OPAMP3) | ADC3_VAL[1] uint16_t | Yes |
| DMA1 Channel 7 | ADC4 (VREFINT + BAT PB12) | ADC4_VAL[2] uint16_t | Yes |

## Communication Interfaces

| Interface | Peripheral | Connected To | Notes |
|-----------|-----------|--------------|-------|
| SPI1 | W25Q128 ext flash | PA5=SCK, PB4=MISO, PB5=MOSI, PB6=CS | Blocking HAL calls |
| SPI2 | ST7789V LCD | PB13=SCK, PB15=MOSI, PA10=CS, PA11=DC, PA12=RST | DMA TX, non-blocking |
| I2C1 | MS5837-30BA | Standard-mode, IT-driven (no DMA) | FSM: RESET->PROM->D2->D1->READ |
| ADC1 | OPAMP1 output | SENSOR1 galvanic O2 cell | 16x oversampling, continuous+DMA |
| ADC2 | OPAMP2 output | SENSOR2 galvanic O2 cell | 16x oversampling, continuous+DMA |
| ADC3 | OPAMP3 output | SENSOR3 galvanic O2 cell | continuous+DMA |
| ADC4 ch1/ch3 | VREFINT + PB12 | Internal ref + battery divider | Scan 2ch, DMA |
| TIM5 CH1 | Vibro motor | PA0 (PWM) | 200 Hz, duty 50/75/99% per warning level |
| TIM2 CH3 | LCD backlight | PA9 (PWM) | Gamma-corrected brightness |
| TIM6 | Scheduler | (internal) | 1 kHz, master for periodic flags |
| GPIO EXTI2/3 | Buttons | PA2 (BTN_M), PA3 (BTN_S) | 50 ms debounce |

## GPIO Pin Reference

| Signal | Pin | Function |
|--------|-----|----------|
| VIB_PWM | PA0 | TIM5 CH1 -- vibro motor PWM |
| SENSOR_1 | PA1 | OPAMP1 input (analog) |
| BTN_M | PA2 | Button Main -- EXTI2 |
| BTN_S | PA3 | Button Select -- EXTI3 |
| FL_SCK | PA5 | SPI1 clock (W25Q128) |
| SENSOR_2 | PA7 | OPAMP2 input (analog) |
| TFT_BLK | PA9 | TIM2 CH3 -- LCD backlight PWM |
| TFT_CS | PA10 | LCD chip-select |
| TFT_DC | PA11 | LCD data/command |
| TFT_RST | PA12 | LCD reset |
| FL_MISO | PB4 | SPI1 MISO (W25Q128) |
| FL_MOSI | PB5 | SPI1 MOSI (W25Q128) |
| FL_CS | PB6 | SPI1 CS (W25Q128) |
| SENSOR_3 | PB0 | OPAMP3 input (analog) |
| SENS_BAT | PB12 | ADC4 ch3 -- battery voltage (3x divider) |
| TFT_SCK | PB13 | SPI2 clock (LCD) |
| TFT_MOSI | PB15 | SPI2 MOSI (LCD) |

## Scheduler Flags (TIM6 @ 1 kHz -> HAL_TIM_PeriodElapsedCallback)

| Flag | Period | Used for |
|------|--------|---------|
| g_flag_5ms | 5 ms | LVGL timer handler, vibro_tick_5ms() |
| g_flag_20ms | 20 ms | Button event dispatch |
| g_flag_1hz | 1 s | Screen update, PPO2 warning check, battery check |
| g_flag_10s | 10 s | Drift tracker sample |

## Shared Variables (ISR <-> main / module <-> module)

| Variable | Type | Writers | Readers | Protection |
|----------|------|---------|---------|------------|
| ADC1_VAL / ADC2_VAL / ADC3_VAL | volatile uint16_t[1] | DMA ISR | sensor.c | atomic flag g_adc1/2/3_new |
| ADC4_VAL | volatile uint16_t[2] | DMA ISR | sensor.c | atomic flag g_adc4_new |
| g_adc1_new .. g_adc4_new | volatile bool / atomic | HAL_ADC_ConvCpltCallback | sensor.c | volatile write, single-reader |
| button event queue | ring buffer | EXTI ISR (button.c) | main loop | volatile head/tail indices |
| sensor_data struct | struct | sensor.c | warning.c, screens | single-writer, read in main loop |
| alarm state | enum | warning.c | screens, vibro FSM | single-writer |
| screen handle | lv_obj_t* | screen_manager.c | screens | single-writer |
| settings struct | struct | settings_storage.c | multiple modules | loaded once at boot, written on save |
| MS5837 FSM state | enum | I2C callbacks (ms5837.c) | pressure_sensor.c | volatile, single-writer |
