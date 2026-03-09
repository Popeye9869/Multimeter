#include "power_buffer.h"
#include "main.h"
#include "gpio.h"
#include "opamp.h"
#include "stm32g4xx.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_opamp.h"

HAL_StatusTypeDef PowerBuffer_Init()
{
    //启动运放跟随器opamp1
    MX_OPAMP1_Init();
    return HAL_OPAMP_Start(&hopamp1);
}

HAL_StatusTypeDef PowerBuffer_SetLevel(PowerBufferLevel_TypeDef level)
{

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    switch (level) {
        case POWER_BUFFER_LEVEL_LOW:
            HAL_OPAMP_Stop(&hopamp1);
            HAL_OPAMP_DeInit(&hopamp1);

            GPIO_InitStruct.Pin = FOLLOWER_OUT_Pin;    
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    
            GPIO_InitStruct.Pull = GPIO_NOPULL;    
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

            HAL_GPIO_Init(FOLLOWER_OUT_GPIO_Port, &GPIO_InitStruct);

            HAL_GPIO_WritePin(FOLLOWER_OUT_GPIO_Port, FOLLOWER_OUT_Pin, GPIO_PIN_RESET);
            break;
        case POWER_BUFFER_LEVEL_MEDIUM:
            // 设置为中等电平

            HAL_GPIO_DeInit(FOLLOWER_OUT_GPIO_Port, FOLLOWER_OUT_Pin);

            PowerBuffer_Init();

            HAL_GPIO_WritePin(PULL1_GPIO_Port, PULL1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PULL2_GPIO_Port, PULL2_Pin, GPIO_PIN_RESET);
            break;
        case POWER_BUFFER_LEVEL_HIGH:
            // 设置为高电平
            HAL_OPAMP_Stop(&hopamp1);
            HAL_OPAMP_DeInit(&hopamp1);

            GPIO_InitStruct.Pin = FOLLOWER_OUT_Pin;    
            GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;    
            GPIO_InitStruct.Pull = GPIO_NOPULL;    
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

            HAL_GPIO_Init(FOLLOWER_OUT_GPIO_Port, &GPIO_InitStruct);

            HAL_GPIO_WritePin(FOLLOWER_OUT_GPIO_Port, FOLLOWER_OUT_Pin, GPIO_PIN_SET);
            break;
        default:
            return HAL_ERROR;
    }
    return HAL_OK;
}
