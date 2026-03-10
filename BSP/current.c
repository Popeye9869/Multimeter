#include "current.h"

#include "power_buffer.h"
#include "dac.h"
#include "stm32g4xx_hal_dac.h"

void SetCurrent(Current_Type  uA)
{
    if(uA == CURRENT_RANGE_N500uA) {
        PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_HIGH);
    } else {
        PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_LOW);
    }
    switch (uA) {
        case CURRENT_RANGE_10uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 82);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_15uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 127);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_150uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 1200);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_400uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 3258);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_500uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 4073);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_N500uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 110);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        default:
            // Handle invalid input if necessary
            break;
    
    }
}