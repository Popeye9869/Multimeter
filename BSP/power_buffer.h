#include "main.h"
#include "gpio.h"
#include "opamp.h"

typedef enum PowerBufferLevel
{
    POWER_BUFFER_LEVEL_LOW = 0,
    POWER_BUFFER_LEVEL_MEDIUM,
    POWER_BUFFER_LEVEL_HIGH
}PowerBufferLevel_TypeDef;


HAL_StatusTypeDef PowerBuffer_Init();

HAL_StatusTypeDef PowerBuffer_SetLevel(PowerBufferLevel_TypeDef level);

