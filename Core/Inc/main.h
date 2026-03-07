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
#define PWM_IN_Pin GPIO_PIN_0
#define PWM_IN_GPIO_Port GPIOC
#define COMP_IN_Pin GPIO_PIN_1
#define COMP_IN_GPIO_Port GPIOA
#define FOLLOWER_OUT_Pin GPIO_PIN_2
#define FOLLOWER_OUT_GPIO_Port GPIOA
#define FOLLOWER_IN_Pin GPIO_PIN_3
#define FOLLOWER_IN_GPIO_Port GPIOA
#define DAC1_OUT1_Pin GPIO_PIN_4
#define DAC1_OUT1_GPIO_Port GPIOA
#define PGA_BIAS_Pin GPIO_PIN_5
#define PGA_BIAS_GPIO_Port GPIOA
#define COMP_OUT_Pin GPIO_PIN_6
#define COMP_OUT_GPIO_Port GPIOA
#define PGA_IN_Pin GPIO_PIN_7
#define PGA_IN_GPIO_Port GPIOA
#define OP_VINP_Pin GPIO_PIN_0
#define OP_VINP_GPIO_Port GPIOB
#define OP_VOUT_Pin GPIO_PIN_1
#define OP_VOUT_GPIO_Port GPIOB
#define OP_VINM_Pin GPIO_PIN_2
#define OP_VINM_GPIO_Port GPIOB
#define BEEP_Pin GPIO_PIN_13
#define BEEP_GPIO_Port GPIOD
#define KEY_Pin GPIO_PIN_15
#define KEY_GPIO_Port GPIOD
#define KEY_EXTI_IRQn EXTI15_10_IRQn
#define R_SW_Pin GPIO_PIN_6
#define R_SW_GPIO_Port GPIOC
#define OLED_SCL_Pin GPIO_PIN_15
#define OLED_SCL_GPIO_Port GPIOA
#define OLED_SDA_Pin GPIO_PIN_7
#define OLED_SDA_GPIO_Port GPIOB
#define PULL1_Pin GPIO_PIN_0
#define PULL1_GPIO_Port GPIOE
#define PULL2_Pin GPIO_PIN_1
#define PULL2_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
