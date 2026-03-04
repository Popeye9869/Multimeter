#include "power_buffer.h"
#include "main.h"
#include "stm32g4xx.h"
#include "stm32g4xx_hal_gpio.h"

HAL_StatusTypeDef PowerBuffer_Init()
{
    //启动运放跟随器opamp1
    return HAL_OPAMP_Start(&hopamp1);
}

HAL_StatusTypeDef PowerBuffer_SetLevel(PowerBufferLevel_TypeDef level)
{
    switch (level) {
        case POWER_BUFFER_LEVEL_LOW:
            // 设置为低电平
            HAL_GPIO_WritePin(PULL1_GPIO_Port, PULL1_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PULL2_GPIO_Port, PULL2_Pin, GPIO_PIN_RESET);
            break;
        case POWER_BUFFER_LEVEL_MEDIUM:
            // 设置为中等电平
            HAL_GPIO_WritePin(PULL1_GPIO_Port, PULL1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PULL2_GPIO_Port, PULL2_Pin, GPIO_PIN_RESET);
            break;
        case POWER_BUFFER_LEVEL_HIGH:
            // 设置为高电平
            HAL_GPIO_WritePin(PULL1_GPIO_Port, PULL1_Pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(PULL2_GPIO_Port, PULL2_Pin, GPIO_PIN_SET);
            break;
        default:
            return HAL_ERROR;
    }
    return HAL_OK;
}
