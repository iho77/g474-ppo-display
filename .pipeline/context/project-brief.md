# Project Brief

## Device Purpose
Closed-circuit rebreather (CCR) PPO2 monitoring and display device with
vibrotactile alarm system. Reads oxygen partial pressure from three galvanic
oxygen sensors via ADC (OPAMP 16× gain), processes readings, displays on
a 320×240 SPI TFT screen, and triggers vibration motor alarms on
hypoxia/hyperoxia threshold violations. Also monitors dive depth via an
MS5837-30BA I2C pressure sensor and tracks long-term PPO2 drift trends.

**Device failure = diver may not receive hypoxia/hyperoxia warning = potential fatality.**

## Hardware
- **MCU**: STM32G474CCTx (Cortex-M4, 96 MHz, 256 KB Flash single-bank, 128 KB RAM)
- **Display**: 320×240 SPI TFT (ST7789V driver, SPI2 DMA)
- **O2 Sensors**: 3× galvanic cells via OPAMP1/2/3 (16× gain) → ADC1/2/3
- **Pressure Sensor**: MS5837-30BA (I2C1, interrupt-driven FSM)
- **Storage**: W25Q128 external SPI NOR flash (16 MB) for settings and calibration
- **Vibro Motor**: TIM5 CH1 PWM (PA0)
- **Buttons**: 2× EXTI (BTN_M=PA2, BTN_S=PA3)
- **Battery Monitor**: ADC4 channel (PB12), 3× voltage divider

## Safety Classification
Life-safety diving equipment. No formal SIL classification but designed
to SIL 2 equivalent principles. Single-fault tolerant for critical alarm path.

## Operating Environment
- Temperature range: 2°C to 40°C (water/ambient)
- Vibration/shock: Moderate (diving equipment handling, wave action)
- EMI exposure: Low (underwater, but potential from other dive electronics)
- Power supply: Battery powered (LiPo; full range characterised via battery monitor)
- Expected continuous runtime: 3-4 hours per dive
- Pressure: Device at surface pressure (not pressure-exposed)
- Moisture: High humidity environment, potential condensation

## Critical Functions (failure = safety hazard)
1. PPO2 reading acquisition and processing (ADC → OPAMP → calibrated value)
2. Alarm threshold comparison and vibrotactile alarm triggering (3-level FSM)
3. Display of current PPO2 values to diver
4. Sensor fault detection (open circuit, short circuit, degraded sensor)

## Important Functions (failure = device unusable but not dangerous)
1. User interface / button handling
2. Two-point sensor calibration routine
3. Display brightness/mode control (TIM2 CH3 PWM backlight)
4. Depth monitoring (MS5837-30BA pressure sensor)
5. PPO2 drift tracking (3-minute linear regression, 10 s samples)
6. Sensor diagnostic / sensitivity degradation detection
7. Settings and calibration persistence (W25Q128 external flash, wear-levelled)
8. Dive log display
