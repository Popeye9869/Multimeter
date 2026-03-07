#include "current.h"

#include "power_buffer.h"
#include "dac.h"
#include "stm32g4xx_hal_dac.h"

void SetCurrent(Current_Type  uA)
{
    switch (uA) {
        case CURRENT_RANGE_10uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2050);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_15uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2088);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_150uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 372);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        case CURRENT_RANGE_500uA:
            HAL_DAC_Stop(&hdac1, DAC_CHANNEL_1);
            HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 3720);
            HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
            break;
        default:
            // Handle invalid input if necessary
            break;
    
    }
}