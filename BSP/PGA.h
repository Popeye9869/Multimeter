#include "main.h"
#include "opamp.h"
#include "stm32g4xx_hal_def.h"

HAL_StatusTypeDef PGA_Init();

HAL_StatusTypeDef PGA_ChangeGain(uint32_t gain);//增益设置函数。
