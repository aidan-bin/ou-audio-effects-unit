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
#include "stm32f3xx_hal.h"

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
#define nNVM_WE_Pin GPIO_PIN_13
#define nNVM_WE_GPIO_Port GPIOC
#define HSE_IN_Pin GPIO_PIN_0
#define HSE_IN_GPIO_Port GPIOF
#define HSE_OUT_Pin GPIO_PIN_1
#define HSE_OUT_GPIO_Port GPIOF
#define POT_D_Pin GPIO_PIN_0
#define POT_D_GPIO_Port GPIOC
#define POT_A_Pin GPIO_PIN_0
#define POT_A_GPIO_Port GPIOA
#define POT_B_Pin GPIO_PIN_1
#define POT_B_GPIO_Port GPIOA
#define POT_C_Pin GPIO_PIN_3
#define POT_C_GPIO_Port GPIOA
#define STATUS_LED_Pin GPIO_PIN_6
#define STATUS_LED_GPIO_Port GPIOC
#define SWITCH_A_Pin GPIO_PIN_7
#define SWITCH_A_GPIO_Port GPIOC
#define SWITCH_B_Pin GPIO_PIN_8
#define SWITCH_B_GPIO_Port GPIOC
#define SWITCH_C_Pin GPIO_PIN_9
#define SWITCH_C_GPIO_Port GPIOC
#define BTN_C_Pin GPIO_PIN_8
#define BTN_C_GPIO_Port GPIOA
#define BTN_C_EXTI_IRQn EXTI9_5_IRQn
#define BTN_B_Pin GPIO_PIN_9
#define BTN_B_GPIO_Port GPIOA
#define BTN_B_EXTI_IRQn EXTI9_5_IRQn
#define BTN_A_Pin GPIO_PIN_10
#define BTN_A_GPIO_Port GPIOA
#define BTN_A_EXTI_IRQn EXTI15_10_IRQn
#define LED_STATUS_Pin GPIO_PIN_2
#define LED_STATUS_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
