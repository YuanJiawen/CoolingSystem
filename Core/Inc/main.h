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
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define SW2_Pin GPIO_PIN_13
#define SW2_GPIO_Port GPIOC
#define SW1_Pin GPIO_PIN_0
#define SW1_GPIO_Port GPIOA
#define ADC_V_CANISTER_PRESS_Pin GPIO_PIN_1
#define ADC_V_CANISTER_PRESS_GPIO_Port GPIOA
#define ADC_V_TUBE_PRESS_Pin GPIO_PIN_2
#define ADC_V_TUBE_PRESS_GPIO_Port GPIOA
#define LED1_Pin GPIO_PIN_10
#define LED1_GPIO_Port GPIOH
#define LED2_Pin GPIO_PIN_11
#define LED2_GPIO_Port GPIOH
#define LED3_Pin GPIO_PIN_12
#define LED3_GPIO_Port GPIOH
#define VALVE_DIAG_IN_Pin GPIO_PIN_8
#define VALVE_DIAG_IN_GPIO_Port GPIOA
#define LEVEL_HIGH_FB_Pin GPIO_PIN_9
#define LEVEL_HIGH_FB_GPIO_Port GPIOA
#define LEVEL_LOW_FB_Pin GPIO_PIN_10
#define LEVEL_LOW_FB_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */
#define TOUCH_INT_Pin GPIO_PIN_13
#define TOUCH_INT_GPIO_Port GPIOD
#define TOUCH_INT_EXTI_IRQn EXTI15_10_IRQn
#define TOUCH_RST_Pin GPIO_PIN_8
#define TOUCH_RST_GPIO_Port GPIOI
#define LCD_PWR_EN_Pin GPIO_PIN_7
#define LCD_PWR_EN_GPIO_Port GPIOD
#define SPI_FLASH_CS_Pin GPIO_PIN_6
#define SPI_FLASH_CS_GPIO_Port GPIOF
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
