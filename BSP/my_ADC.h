#include "main.h"
#include "math.h"
#include "adc.h"
#include "stm32g4xx_hal_def.h"
#include <stdint.h>

#define ADC_BUFFER_LENGTH 56000// 定义ADC缓冲区长度
#define ADC_BUFFER_DC_LENGTH 200// 定义用于直流测量的ADC缓冲区长度
#define ADC_BUFFER_CONT_LENGTH 1// 定义用于通断档的ADC缓冲区长度，优先响应速度
#define ADC_AC_ZERO_LEVEL 4096U// 交流测量固定零电位，对应12位ADC中点（两倍超采样）

#define DC_20V_Calc(adc_raw_16_65535_levelMid) ((((adc_raw_16_65535_levelMid / 65535.0) * 3.3-1.65)*1000/86)+0.010925)/0.97
#define DC_2000mV_Calc(adc_raw_16_65535_levelMid) ((((adc_raw_16_65535_levelMid / 65535.0) * 3.3-1.65)*1000/(43*16))+0.01028315)/0.9667

extern uint16_t adc_value[ADC_BUFFER_LENGTH]; // 定义ADC值数组
extern uint16_t adc_after_filter; // 定义全局变量存储滤波后的ADC值
extern double rms_voltage; // 定义全局变量存储交流电压的真有效值

extern enum ADV_MODE
{
    DC_MODE,
    AC_MODE
} ADC_MODE;

HAL_StatusTypeDef ADC_SetDCMode();
HAL_StatusTypeDef ADC_SetACMode();
HAL_StatusTypeDef ADC_StartDC_DMA(uint32_t sample_count);
