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
#define COMP_OUT_Pin GPIO_PIN_0
#define COMP_OUT_GPIO_Port GPIOA
#define COMP_IN_Pin GPIO_PIN_1
#define COMP_IN_GPIO_Port GPIOA
#define FOLLOWER_OUT_Pin GPIO_PIN_2
#define FOLLOWER_OUT_GPIO_Port GPIOA
#define FOLLOWER_IN_Pin GPIO_PIN_3
#define FOLLOWER_IN_GPIO_Port GPIOA
#define OP_VINP_Pin GPIO_PIN_0
#define OP_VINP_GPIO_Port GPIOB
#define OP_VOUT_Pin GPIO_PIN_1
#define OP_VOUT_GPIO_Port GPIOB
#define OP_VINM_Pin GPIO_PIN_2
#define OP_VINM_GPIO_Port GPIOB
#define PGA_IN_Pin GPIO_PIN_14
#define PGA_IN_GPIO_Port GPIOB
#define KEY_Pin GPIO_PIN_15
#define KEY_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
