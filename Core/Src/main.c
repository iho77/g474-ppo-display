/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdatomic.h>
#include <stdbool.h>
#include "stm32g4xx_ll_adc.h"
#include "stm32g4xx.h"
#include "sensor.h"
#include "lvgl.h"
#include "st7789v.h"
#include "screen_manager.h"
#include "button.h"
#include "flash_storage.h"
#include "settings_storage.h"
#include "ext_flash_storage.h"
#include "drift_tracker.h"
#include "ms5837.h"
#include "w25q128.h"
#include "warning.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */




// Display configuration (landscape orientation)
#define LCD_H_RES       320
#define LCD_V_RES       240
#define DISP_BUF_LINES  20  // Number of lines in partial refresh buffer

// LVGL display buffer - single buffer for partial refresh mode
// RGB565 = 2 bytes per pixel, 10 lines = 320 * 10 * 2 = 6400 bytes
static lv_color_t disp_buf1[LCD_H_RES * DISP_BUF_LINES] = {0};
static lv_color_t disp_buf2[LCD_H_RES * DISP_BUF_LINES] = {0};

// LVGL v8 display driver structures (must be global for ISR access from other files)
static lv_disp_draw_buf_t disp_draw_buf;
lv_disp_drv_t disp_drv;  // Non-static so ISR can access it

static volatile uint16_t ADC1_VAL[1];   // SENSOR1 via VOPAMP1
static volatile uint16_t ADC2_VAL[1];   // SENSOR2 via VOPAMP2
static volatile uint16_t ADC3_VAL[1];   // SENSOR3 via VOPAMP3
static volatile uint16_t ADC4_VAL[2];   // [0]=VREFINT  [1]=BAT (PB12)
static volatile atomic_bool g_adc1_new = false;
static volatile atomic_bool g_adc2_new = false;
static volatile atomic_bool g_adc3_new = false;
static volatile atomic_bool g_adc4_new = false;

// Elapsed time counter (seconds since boot)
uint32_t g_elapsed_time_sec = 0;
/* Bottom timer start depth in mm. 1000 mm = 1 m. Change for bench testing. */
#define BOTTOM_TIMER_START_DEPTH_MM  1000


static volatile uint32_t g_ms = 0; // ms since boot (from 1kHz tick)
static volatile uint32_t g_ms1 = 0;

static volatile atomic_bool g_flag_1hz = false;
static volatile atomic_bool g_flag_5ms = false;
static volatile atomic_bool g_flag_20ms = false;
static volatile atomic_bool g_flag_10s = false;  // NEW: 10-second flag for drift tracking


/**
 * Gamma 2.2 Lookup Table for Backlight PWM
 * Clock: 48 MHz | Prescaler: 0 | ARR: 4799 (10 kHz PWM)
 * Index 0 = OFF, Index 1 = 10% ... Index 10 = 100%
 * Provides perceptually linear brightness control
 */
const uint16_t backlight_gamma_lut[11] = {
      0,   // 0%   (OFF)
     30,   // 10%
    139,   // 20%
    339,   // 30%
    639,   // 40%
   1044,   // 50%
   1561,   // 60%
   2190,   // 70%
   2937,   // 80%
   3808,   // 90%
   4799    // 100% (MAX)
};

uint32_t vref_effective_mv = 3300;     // == VDDA in mV

// OPAMP offset calibration results (volatile to prevent optimizer from removing)
volatile uint16_t offset_counts_ch1 = 0;  // for OPAMP2 -> ADC_CHANNEL_VOPAMP2
volatile uint16_t offset_counts_ch2 = 0; // for OPAMP3 -> ADC_CHANNEL_VOPAMP3_ADC2

// Debug: Store VREFINT raw reading for troubleshooting
uint16_t vrefint_raw_debug = 0;

// MS5837 pressure sensor calibration and data
static ms5837_calib_t ms5837_calib;  // Factory calibration coefficients
static ms5837_data_t ms5837_data;    // Latest pressure/temperature measurement
static volatile bool ms5837_initialized = false;  // Initialization status flag

/* MS5837 DMA-driven measurement FSM */
typedef enum {
    MS5837_FSM_IDLE = 0,
    MS5837_FSM_D2_CONV_TX,   /* D2 conversion command TX in progress */
    MS5837_FSM_WAIT_D2,
    MS5837_FSM_D2_READ_TX,
    MS5837_FSM_D2_READ_RX,
    MS5837_FSM_D1_CMD_TX,
    MS5837_FSM_WAIT_D1,
    MS5837_FSM_D1_READ_TX,
    MS5837_FSM_D1_READ_RX,
    MS5837_FSM_CALCULATE,
} ms5837_fsm_t;

static volatile ms5837_fsm_t  ms5837_fsm        = MS5837_FSM_IDLE;
static volatile uint32_t      ms5837_conv_start = 0;
static volatile uint32_t      ms5837_raw_D1     = 0;
static volatile uint32_t      ms5837_raw_D2     = 0;
static volatile uint32_t      ms5837_last_meas  = 0;
/* Static buffers required: DMA accesses them after function returns */
static uint8_t       ms5837_tx_buf[1]  = {0};
static uint8_t       ms5837_rx_buf[3]  = {0, 0, 0};

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
ADC_HandleTypeDef hadc4;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;
DMA_HandleTypeDef hdma_adc3;
DMA_HandleTypeDef hdma_adc4;

I2C_HandleTypeDef hi2c1;

IWDG_HandleTypeDef hiwdg;

OPAMP_HandleTypeDef hopamp1;
OPAMP_HandleTypeDef hopamp2;
OPAMP_HandleTypeDef hopamp3;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim5;
TIM_HandleTypeDef htim6;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_ADC3_Init(void);
static void MX_ADC4_Init(void);
static void MX_I2C1_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM5_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM3_Init(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
void vibro_motor_test(void);

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM6_Init(void);
static void MX_ADC1_Init(void);
static void MX_OPAMP3_Init(void);
static void MX_OPAMP2_Init(void);
static void MX_TIM3_Init(void);
static void MX_OPAMP1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
void ui_init(lv_disp_t *disp);
void LVGL_Task(void const *argument);

static void scheduler_tick_1kHz(void);
static void scheduler_tick_5ms(void);
static void scheduler_tick_20ms(void);
static void scheduler_tick_10s(void);

HAL_StatusTypeDef adc2_calculate_offsets_two_gain(void);
void system_shutdown(void);  // Power management


/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


uint8_t isDebuggerConnected(void) {
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk) {
        return 1; // Debugger is attached
    }
    return 0; // Running standalone
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		scheduler_tick_1kHz();
		scheduler_tick_5ms();
		scheduler_tick_20ms();
		scheduler_tick_10s();  // NEW: 10-second scheduler for drift tracking
	}
}


static void scheduler_tick_5ms(void){
	g_ms1++;

		static uint32_t t1 = 0;
		if ((g_ms1 - t1) >= 5) {
			t1 = g_ms1;
			atomic_store(&g_flag_5ms, true);
		}

}

static void scheduler_tick_20ms(void) {
	static uint32_t t = 0;
	if ((g_ms - t) >= 20) {
		t = g_ms;
		atomic_store(&g_flag_20ms, true);
	}
}

static void scheduler_tick_1kHz(void) {
	g_ms++;

	// 1 Hz: every 1000 ms
	static uint32_t t1 = 0;
	if ((g_ms - t1) >= 1000) {
		t1 = g_ms;
		atomic_store(&g_flag_1hz, true);
	}
}

static void scheduler_tick_10s(void) {
	static uint32_t t = 0;
	if ((g_ms - t) >= 10000) {  // 10 seconds = 10000 ms
		t = g_ms;
		atomic_store(&g_flag_10s, true);
	}
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == ADC1) atomic_store(&g_adc1_new, true);
    if (hadc->Instance == ADC2) atomic_store(&g_adc2_new, true);
    if (hadc->Instance == ADC3) atomic_store(&g_adc3_new, true);
    if (hadc->Instance == ADC4) atomic_store(&g_adc4_new, true);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI2) {
    //    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
     //   my_disp_bus_busy = 0;

        // v8 API: pass driver struct pointer
        lv_disp_flush_ready(&disp_drv);
    }
}

/**
 * @brief System shutdown - Stop peripherals and enter low-power mode
 * @note Wakes on PA8 (BTN_M) press, performs full system reset
 */
void system_shutdown(void) {
    // Step 1: Turn off LCD backlight (TIM2_CH3 on PA9)
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);

    // Reconfigure PA2 (TIM2_CH3 backlight) as GPIO output LOW
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);

    // Step 2: Stop all peripherals
    HAL_ADC_Stop_DMA(&hadc1);
    HAL_ADC_Stop_DMA(&hadc2);
    HAL_TIM_Base_Stop_IT(&htim6);  // 1kHz scheduler
    HAL_TIM_Base_Stop(&htim3);     // ADC2 trigger
    HAL_OPAMP_Stop(&hopamp1);
    HAL_OPAMP_Stop(&hopamp2);
    HAL_OPAMP_Stop(&hopamp3);

    // Step 3: Set GPIO to low-power state
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);

    // Step 4: Configure wake-up source (PA8 via EXTI)
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_8);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

    // Step 5: Enter Stop1 mode (~2 µA)
    HAL_SuspendTick();
    HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI);

    // Step 6: Wake-up handling (after BTN_M press)
    SystemClock_Config();  // Restore clocks after Stop mode
    HAL_ResumeTick();
    NVIC_SystemReset();    // Full restart for clean boot
}

/**
 * @brief MS5837 main-loop tick — handles timer-wait states and CALCULATE.
 * All other state transitions happen inside DMA callbacks (ISR context).
 */
static void ms5837_fsm_tick(void)
{
    if (!ms5837_initialized) return;

    switch (ms5837_fsm) {

        case MS5837_FSM_IDLE:
            if ((g_ms - ms5837_last_meas) >= 1000U) {
                ms5837_tx_buf[0] = MS5837_CMD_CONV_D2 + MS5837_OSR_4096;
                if (HAL_I2C_Master_Transmit_IT(&hi2c1,
                        (uint16_t)(MS5837_I2C_ADDR << 1),
                        ms5837_tx_buf, 1U) == HAL_OK) {
                    ms5837_fsm = MS5837_FSM_D2_CONV_TX;  /* TxCplt advances to WAIT_D2 */
                } else {
                    ms5837_last_meas = g_ms;  /* back-off 1 s on bus error */
                }
            }
            break;

        case MS5837_FSM_WAIT_D2:
            if ((g_ms - ms5837_conv_start) >= MS5837_CONV_DELAY_4096) {
                ms5837_tx_buf[0] = MS5837_CMD_ADC_READ;
                if (HAL_I2C_Master_Transmit_IT(&hi2c1,
                        (uint16_t)(MS5837_I2C_ADDR << 1),
                        ms5837_tx_buf, 1U) == HAL_OK) {
                    ms5837_fsm = MS5837_FSM_D2_READ_TX;
                }
            }
            break;

        case MS5837_FSM_WAIT_D1:
            if ((g_ms - ms5837_conv_start) >= MS5837_CONV_DELAY_4096) {
                ms5837_tx_buf[0] = MS5837_CMD_ADC_READ;
                if (HAL_I2C_Master_Transmit_IT(&hi2c1,
                        (uint16_t)(MS5837_I2C_ADDR << 1),
                        ms5837_tx_buf, 1U) == HAL_OK) {
                    ms5837_fsm = MS5837_FSM_D1_READ_TX;
                }
            }
            break;

        case MS5837_FSM_CALCULATE:
            ms5837_calculate(&ms5837_calib, ms5837_raw_D1, ms5837_raw_D2, &ms5837_data);
            {
                int32_t scaled_p = ms5837_data.pressure_mbar * 100;
                sensor_data.pressure.pressure_mbar = scaled_p;
                sensor_data.pressure.temperature_c =
                    (int16_t)(ms5837_data.temperature_c100 / 100);
                sensor_data.pressure.depth_mm =
                    pressure_sensor_calculate_depth_mm(
                        scaled_p, sensor_data.pressure.surface_pressure_mbar);
                sensor_data.pressure.pressure_valid = true;
            }
            ms5837_last_meas = g_ms;
            ms5837_fsm = MS5837_FSM_IDLE;
            break;

        default:
            break;  /* TX/RX states: DMA in progress, nothing to do */
    }
}

void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    switch (ms5837_fsm) {
        case MS5837_FSM_D2_CONV_TX:
            /* D2 conversion command sent — start conversion timer */
            ms5837_conv_start = g_ms;
            ms5837_fsm = MS5837_FSM_WAIT_D2;
            break;

        case MS5837_FSM_D2_READ_TX:
            /* ADC read command for D2 sent — start 3-byte IT RX */
            if (HAL_I2C_Master_Receive_IT(hi2c,
                    (uint16_t)(MS5837_I2C_ADDR << 1), ms5837_rx_buf, 3U) == HAL_OK) {
                ms5837_fsm = MS5837_FSM_D2_READ_RX;
            } else {
                ms5837_fsm = MS5837_FSM_IDLE;
                ms5837_last_meas = g_ms;
            }
            break;

        case MS5837_FSM_D1_CMD_TX:
            /* D1 conversion command sent — start conversion timer */
            ms5837_conv_start = g_ms;
            ms5837_fsm = MS5837_FSM_WAIT_D1;
            break;

        case MS5837_FSM_D1_READ_TX:
            /* ADC read command for D1 sent — start 3-byte IT RX */
            if (HAL_I2C_Master_Receive_IT(hi2c,
                    (uint16_t)(MS5837_I2C_ADDR << 1), ms5837_rx_buf, 3U) == HAL_OK) {
                ms5837_fsm = MS5837_FSM_D1_READ_RX;
            } else {
                ms5837_fsm = MS5837_FSM_IDLE;
                ms5837_last_meas = g_ms;
            }
            break;

        default:
            break;
    }
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;

    switch (ms5837_fsm) {
        case MS5837_FSM_D2_READ_RX:
            /* Save D2 raw ADC value */
            ms5837_raw_D2 = ((uint32_t)ms5837_rx_buf[0] << 16)
                          | ((uint32_t)ms5837_rx_buf[1] << 8)
                          |  (uint32_t)ms5837_rx_buf[2];
            /* Start D1 conversion command */
            ms5837_tx_buf[0] = MS5837_CMD_CONV_D1 + MS5837_OSR_4096;
            if (HAL_I2C_Master_Transmit_IT(hi2c,
                    (uint16_t)(MS5837_I2C_ADDR << 1),
                    ms5837_tx_buf, 1U) == HAL_OK) {
                ms5837_fsm = MS5837_FSM_D1_CMD_TX;
            } else {
                ms5837_fsm = MS5837_FSM_IDLE;
                ms5837_last_meas = g_ms;
            }
            break;

        case MS5837_FSM_D1_READ_RX:
            /* Save D1 raw ADC value — hand off to main loop for calculation */
            ms5837_raw_D1 = ((uint32_t)ms5837_rx_buf[0] << 16)
                          | ((uint32_t)ms5837_rx_buf[1] << 8)
                          |  (uint32_t)ms5837_rx_buf[2];
            ms5837_fsm = MS5837_FSM_CALCULATE;
            break;

        default:
            break;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance != I2C1) return;
    /* Any I2C error (NACK, BERR, ARLO, TIMEOUT) resets the FSM.
       hi2c->State is already READY when HAL calls this. */
    ms5837_fsm = MS5837_FSM_IDLE;
    ms5837_last_meas = g_ms;  /* back off 1 s before next attempt */
}

/* MS5837 blocking diagnostic — remove after I2C confirmed working.
 * dbg_step increments after EVERY individual TX or RX so the exact
 * failing operation is visible in the debugger watch window.
 *
 * Step map:
 *   1  = Reset TX ok
 *   2  = PROM TX ok (last word, i=6)        [C[0..6] populated]
 *   3  = Conv-D2 TX ok
 *   4  = 20 ms delay done
 *   5  = ADC-READ TX ok  (0x00 sent)         <-- key sub-step
 *   6  = ADC-READ RX ok  (D2 populated)
 *   7  = Conv-D1 TX ok
 *   8  = 20 ms delay done
 *   9  = ADC-READ TX ok  (0x00 sent)
 *  10  = ADC-READ RX ok  (D1 populated)  → all good, problem is DMA/IT
 *
 * dbg_hal_err: hi2c1.ErrorCode at point of failure.
 *   0x04 = NACK (AF), 0x20 = timeout, 0x01 = bus error.
 */



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_ADC4_Init();
  MX_I2C1_Init();
  MX_OPAMP1_Init();
  MX_OPAMP2_Init();
  MX_OPAMP3_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();
  MX_TIM3_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */

  /* DEBUG: uncomment to test vibro motor hardware before main loop */
//  vibro_motor_test();

  // NOTE: Do NOT lock OPAMPs yet - need to change gain for offset calibration

  	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
  	HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
  	HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
  	HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);



  	HAL_OPAMP_Start(&hopamp1);
  	HAL_OPAMP_Start(&hopamp2);
  	HAL_OPAMP_Start(&hopamp3);

  	HAL_OPAMP_Lock(&hopamp2);
  	HAL_OPAMP_Lock(&hopamp3);
  	HAL_OPAMP_Lock(&hopamp1);

  	HAL_TIM_Base_Start_IT(&htim6);
  	//HAL_TIM_Base_Start(&htim2);
  	// HAL_ADC_Start_IT(&hadc2);
  //	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) ADC1_VAL, 1);
  	HAL_ADC_Start_DMA(&hadc2, (uint32_t*) ADC2_VAL, 1);
  	HAL_ADC_Start_DMA(&hadc3, (uint32_t*) ADC3_VAL, 1);
  	HAL_ADC_Start_DMA(&hadc4, (uint32_t*) ADC4_VAL, 2);


  	// SPI2_LoopbackTest();


  	w25q128_init(&hspi1);     /* CS_HIGH + software reset before first use */


  	sensor_init();
  	pressure_sensor_init();  // Initialize pressure sensor data structure


  	// Initialize MS5837 pressure sensor (blocking, one-time at boot — PROM only)
  	// First measurement comes from the DMA FSM within ~1s
  	if (ms5837_init(&hi2c1, &ms5837_calib, &ms5837_data) == MS5837_OK) {
  	    ms5837_initialized = true;
  	} else {
  	    sensor_data.pressure.pressure_valid = false; /* prevent phantom depth timer on sensor fault */
  	}

  	/* Initialize external flash storage (W25Q128 + calibration log scan) */
  	ext_flash_storage_init(&hspi1);

  	// DIAGNOSTIC: Uncomment next line to erase settings and start fresh
  	// settings_erase();

  	// Load settings from flash (use defaults if not found)
  	if (!settings_load((settings_data_t*)&sensor_data.settings)) {
  		settings_init_defaults((settings_data_t*)&sensor_data.settings);
  	}
  	// Validate loaded settings (clamp to valid ranges)

  	settings_validate((settings_data_t*)&sensor_data.settings);

  	// Apply LCD brightness via PWM (TIM2 CH1 on PA15) with gamma correction
  	uint8_t brightness = sensor_data.settings.lcd_brightness;
  	uint8_t gamma_index = brightness / 10;  // 10% → index 1, 100% → index 10
  	if (gamma_index > 10) gamma_index = 10;  // Clamp to max index
  	uint16_t pwm_value = backlight_gamma_lut[gamma_index];

  	// Set compare value before starting PWM
  	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, pwm_value);

  	// Start PWM output
  	if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3) != HAL_OK) {
  		Error_Handler();
  	}

  		// Initialize ST7789V LCD hardware
  		ST7789V_Init();
  		// Backlight controlled by TIM2 PWM (see above)



  		// Initialize LVGL
  		lv_init();

  		// v8: Initialize draw buffer
  		lv_disp_draw_buf_init(&disp_draw_buf, disp_buf1, disp_buf2,
  		                       LCD_H_RES * DISP_BUF_LINES);

  		// v8: Initialize and register display driver
  		lv_disp_drv_init(&disp_drv);
  		disp_drv.hor_res = LCD_H_RES;
  		disp_drv.ver_res = LCD_V_RES;
  		disp_drv.flush_cb = ST7789V_Flush_Cb;
  		disp_drv.draw_buf = &disp_draw_buf;

  		lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
  		ST7789V_SetDisplay(disp);

  		// v8: lv_scr_act() instead of lv_screen_active()
  	//	lv_obj_t *screen = lv_scr_act();
  	//	lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
  		//lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

  		// Initialize button handling
  		button_init();

  		// Initialize screen manager (loads startup screen)
  		screen_manager_init();

  		// Trigger initial render
  		lv_timer_handler();




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

	while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		ms5837_fsm_tick();

		if (atomic_exchange(&g_flag_5ms, false)) {

			lv_timer_handler();
			vibro_tick_5ms();

		}

		if (atomic_exchange(&g_flag_20ms, false)) {
			// button_poll() removed - buttons now use EXTI interrupt
			btn_event_t evt = button_get_event();
			if (evt != BTN_NONE) {
				screen_manager_handle_button(evt);
			}
		}

		if (atomic_exchange(&g_adc1_new, false)) {

			sensor_data_update(SENSOR1, ADC1_VAL[0]);

		}

		if (atomic_exchange(&g_adc2_new, false)) {

			sensor_data_update(SENSOR2, ADC2_VAL[0]);

		}

		if (atomic_exchange(&g_adc3_new, false)) {

			sensor_data_update(SENSOR3, ADC3_VAL[0]);

		}

		if (atomic_exchange(&g_adc4_new, false)) {

			/* Copy both DMA values atomically — ADC4 DMA runs continuously in circular mode;
			 * reading [0] then [1] without protection risks a torn pair across a half-transfer */
			__disable_irq();
			uint16_t adc4_vref = ADC4_VAL[0];
			uint16_t adc4_bat  = ADC4_VAL[1];
			__enable_irq();
			sensor_data_update(REFERENCE, adc4_vref);
			sensor_data_update(BAT, adc4_bat);

		}

		if (atomic_exchange(&g_flag_1hz, false)) {
			HAL_IWDG_Refresh(&hiwdg);
			/* Bottom timer: advance only when depth >= threshold */
			if (sensor_data.pressure.pressure_valid
					&& sensor_data.pressure.depth_mm
							>= BOTTOM_TIMER_START_DEPTH_MM) {
				g_elapsed_time_sec++;
			}
			screen_manager_update();
			/* Auto-shutdown: battery < 10% for 3 consecutive seconds */
			static uint8_t low_bat_count = 0;
			if (sensor_data.battery_percentage < 10U
					&& isDebuggerConnected() == 0) {
				low_bat_count++;
				if (low_bat_count >= 3U) {
					//HAL_Delay(500);
					//system_shutdown();
				}
			} else {
				low_bat_count = 0;
			}
		}

		if (atomic_exchange(&g_flag_10s, false)) {
			// Sample PPO for drift tracking (every 10 seconds)
			for (int i = 0; i < (SENSOR_COUNT - 2); i++) {
				if (sensor_data.s_valid[i]) {

					drift_tracker_sample(&sensor_data.drift[i],
							sensor_data.o2_s_ppo[i]);

				}
			}
		}

		if (!isDebuggerConnected()) {
			__WFI();
		}
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 12;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = ENABLE;
  hadc1.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
  hadc1.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc1.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc1.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = ENABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = ENABLE;
  hadc2.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
  hadc2.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc2.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc2.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.GainCompensation = 0;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = ENABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.DMAContinuousRequests = ENABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.OversamplingMode = ENABLE;
  hadc3.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
  hadc3.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc3.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc3.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VOPAMP3_ADC3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief ADC4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC4_Init(void)
{

  /* USER CODE BEGIN ADC4_Init 0 */

  /* USER CODE END ADC4_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC4_Init 1 */

  /* USER CODE END ADC4_Init 1 */

  /** Common config
  */
  hadc4.Instance = ADC4;
  hadc4.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc4.Init.Resolution = ADC_RESOLUTION_12B;
  hadc4.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc4.Init.GainCompensation = 0;
  hadc4.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc4.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc4.Init.LowPowerAutoWait = DISABLE;
  hadc4.Init.ContinuousConvMode = ENABLE;
  hadc4.Init.NbrOfConversion = 2;
  hadc4.Init.DiscontinuousConvMode = DISABLE;
  hadc4.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc4.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc4.Init.DMAContinuousRequests = ENABLE;
  hadc4.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc4.Init.OversamplingMode = ENABLE;
  hadc4.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_16;
  hadc4.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc4.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc4.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC4_Init 2 */

  /* USER CODE END ADC4_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x20B17DB6;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 2999;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief OPAMP1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP1_Init(void)
{

  /* USER CODE BEGIN OPAMP1_Init 0 */

  /* USER CODE END OPAMP1_Init 0 */

  /* USER CODE BEGIN OPAMP1_Init 1 */

  /* USER CODE END OPAMP1_Init 1 */
  hopamp1.Instance = OPAMP1;
  hopamp1.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp1.Init.Mode = OPAMP_PGA_MODE;
  hopamp1.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp1.Init.InternalOutput = ENABLE;
  hopamp1.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp1.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp1.Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
  hopamp1.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP1_Init 2 */

  /* USER CODE END OPAMP1_Init 2 */

}

/**
  * @brief OPAMP2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP2_Init(void)
{

  /* USER CODE BEGIN OPAMP2_Init 0 */

  /* USER CODE END OPAMP2_Init 0 */

  /* USER CODE BEGIN OPAMP2_Init 1 */

  /* USER CODE END OPAMP2_Init 1 */
  hopamp2.Instance = OPAMP2;
  hopamp2.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp2.Init.Mode = OPAMP_PGA_MODE;
  hopamp2.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp2.Init.InternalOutput = ENABLE;
  hopamp2.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp2.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp2.Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
  hopamp2.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP2_Init 2 */

  /* USER CODE END OPAMP2_Init 2 */

}

/**
  * @brief OPAMP3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_OPAMP3_Init(void)
{

  /* USER CODE BEGIN OPAMP3_Init 0 */

  /* USER CODE END OPAMP3_Init 0 */

  /* USER CODE BEGIN OPAMP3_Init 1 */

  /* USER CODE END OPAMP3_Init 1 */
  hopamp3.Instance = OPAMP3;
  hopamp3.Init.PowerMode = OPAMP_POWERMODE_NORMALSPEED;
  hopamp3.Init.Mode = OPAMP_PGA_MODE;
  hopamp3.Init.NonInvertingInput = OPAMP_NONINVERTINGINPUT_IO0;
  hopamp3.Init.InternalOutput = ENABLE;
  hopamp3.Init.TimerControlledMuxmode = OPAMP_TIMERCONTROLLEDMUXMODE_DISABLE;
  hopamp3.Init.PgaConnect = OPAMP_PGA_CONNECT_INVERTINGINPUT_NO;
  hopamp3.Init.PgaGain = OPAMP_PGA_GAIN_16_OR_MINUS_15;
  hopamp3.Init.UserTrimming = OPAMP_TRIMMING_FACTORY;
  if (HAL_OPAMP_Init(&hopamp3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN OPAMP3_Init 2 */

  /* USER CODE END OPAMP3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4799;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM5_Init(void)
{

  /* USER CODE BEGIN TIM5_Init 0 */

  /* USER CODE END TIM5_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM5_Init 1 */

  /* USER CODE END TIM5_Init 1 */
  htim5.Instance = TIM5;
  htim5.Init.Prescaler = 0;
  htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim5.Init.Period = 4294967295;
  htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim5) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim5, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM5_Init 2 */

  /* USER CODE END TIM5_Init 2 */
  HAL_TIM_MspPostInit(&htim5);

}

/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 95;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 65535;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */
  /* Override CubeMX period: 96MHz / (96 * 1000) = 1000 Hz exactly */
  htim6.Init.Period = 999;
  HAL_TIM_Base_Init(&htim6);
  /* USER CODE END TIM6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, TFT_CS_Pin|TFT_DC_Pin|TFT_RST_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(FL_CS_GPIO_Port, FL_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : PC13 PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PF0 PF1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PG10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_M_Pin BTN_S_Pin */
  GPIO_InitStruct.Pin = BTN_M_Pin|BTN_S_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA6 PA8 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_6|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB1 PB2 PB10 PB11
                           PB14 PB3 PB8 PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_14|GPIO_PIN_3|GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_CS_Pin TFT_DC_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin|TFT_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_RST_Pin */
  GPIO_InitStruct.Pin = TFT_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(TFT_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : FL_CS_Pin */
  GPIO_InitStruct.Pin = FL_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(FL_CS_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Reconfigure BTN_M (PA2) and BTN_S (PA3) as rising-edge EXTI inputs */
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  HAL_NVIC_SetPriority(EXTI2_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);
  HAL_NVIC_SetPriority(EXTI3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
 * @brief Vibro motor hardware test — exercises PA0 (TIM5 CH1 PWM) at
 *        various pulse lengths and duty cycles.
 *        Call manually in USER CODE BEGIN 2 when debugging hardware.
 *
 * Timer maths: 96 MHz / (PSC+1=960) = 100 kHz tick; ARR+1=500 → 200 Hz PWM.
 * CCR1 sets pulse width: CCR1/500 = duty cycle (0..499).
 */
void vibro_motor_test(void)
{
    /* 200 Hz PWM: 96 MHz / 960 / 500 = 200 Hz */
    __HAL_TIM_SET_PRESCALER(&htim5, 959);
    __HAL_TIM_SET_AUTORELOAD(&htim5, 499);

    /* Short burst – 25% duty, 300 ms */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, 124);
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
    HAL_Delay(300);
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
    HAL_Delay(300);

    /* Medium burst – 50% duty, 500 ms */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, 249);
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
    HAL_Delay(500);
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
    HAL_Delay(300);

    /* Long burst – 75% duty, 800 ms */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, 374);
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
    HAL_Delay(800);
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
    HAL_Delay(300);

    /* Full power – 99% duty, 1000 ms */
    __HAL_TIM_SET_COMPARE(&htim5, TIM_CHANNEL_1, 494);
    HAL_TIM_PWM_Start(&htim5, TIM_CHANNEL_1);
    HAL_Delay(1000);
    HAL_TIM_PWM_Stop(&htim5, TIM_CHANNEL_1);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
	  system_shutdown();
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
