#include "main.h"
#include "math.h"
#include "adc.h"
#include "stm32g4xx_hal_def.h"

#define ADC_BUFFER_LENGTH 56000// 定义ADC缓冲区长度

#define DC_20V_Calc(adc_raw_16_65535_levelMid) ((((adc_raw_16_65535_levelMid / 65535.0) * 3.3-1.65)*1000/86)+0.1856)/0.9601
#define DC_2000mV_Calc(adc_raw_16_65535_levelMid) (((adc_raw_16_65535_levelMid / 65535.0) * 3.3-1.65)*1000/(43*16))

extern uint16_t adc_value[ADC_BUFFER_LENGTH]; // 定义ADC值数组
extern double rms_voltage; // 定义全局变量存储交流电压的真有效值

extern enum ADV_MODE
{
    DC_MODE,
    AC_MODE
} ADC_MODE;

HAL_StatusTypeDef ADC_SetDCMode();
HAL_StatusTypeDef ADC_SetACMode();
