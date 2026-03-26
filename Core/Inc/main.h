/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32g4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
// Backlight gamma 2.2 lookup table (10 kHz PWM, ARR=4799)
extern const uint16_t backlight_gamma_lut[11];

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern TIM_HandleTypeDef htim5;  /* VIB_PWM (PA0) — vibro motor */
void system_shutdown(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VIB_PWM_Pin GPIO_PIN_0
#define VIB_PWM_GPIO_Port GPIOA
#define SENSOR_1_Pin GPIO_PIN_1
#define SENSOR_1_GPIO_Port GPIOA
#define BTN_M_Pin GPIO_PIN_2
#define BTN_M_GPIO_Port GPIOA
#define BTN_S_Pin GPIO_PIN_3
#define BTN_S_GPIO_Port GPIOA
#define FL_SCK_Pin GPIO_PIN_5
#define FL_SCK_GPIO_Port GPIOA
#define SENSOR_2_Pin GPIO_PIN_7
#define SENSOR_2_GPIO_Port GPIOA
#define SENSOR_3_Pin GPIO_PIN_0
#define SENSOR_3_GPIO_Port GPIOB
#define SENS_BAT_Pin GPIO_PIN_12
#define SENS_BAT_GPIO_Port GPIOB
#define TFT_SCK_Pin GPIO_PIN_13
#define TFT_SCK_GPIO_Port GPIOB
#define TFT_MOSI_Pin GPIO_PIN_15
#define TFT_MOSI_GPIO_Port GPIOB
#define TFT_BLK_Pin GPIO_PIN_9
#define TFT_BLK_GPIO_Port GPIOA
#define TFT_CS_Pin GPIO_PIN_10
#define TFT_CS_GPIO_Port GPIOA
#define TFT_DC_Pin GPIO_PIN_11
#define TFT_DC_GPIO_Port GPIOA
#define TFT_RST_Pin GPIO_PIN_12
#define TFT_RST_GPIO_Port GPIOA
#define FL_MISO_Pin GPIO_PIN_4
#define FL_MISO_GPIO_Port GPIOB
#define FL_MOSI_Pin GPIO_PIN_5
#define FL_MOSI_GPIO_Port GPIOB
#define FL_CS_Pin GPIO_PIN_6
#define FL_CS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
