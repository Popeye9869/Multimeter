#include "my_ADC.h"
#include "stm32g4xx_hal_adc.h"
#include <math.h>
#include <stdint.h>
#include "SEGGER_RTT.h"

#include "arm_math.h" // 包含CMSIS DSP库头文件


double rms_voltage = 0.0; // 定义全局变量存储交流电压的真有效值

uint16_t adc_after_filter = 0; // 定义全局变量存储滤波后的ADC值
uint16_t adc_value[ADC_BUFFER_LENGTH]; // 定义ADC值数组
enum ADV_MODE ADC_MODE = DC_MODE; // 默认设置为直流测量模式

HAL_StatusTypeDef ADC_SetDCMode()
{
    HAL_ADC_Stop_DMA(&hadc2); // 停止ADC DMA传输

    MX_ADC2_Init(); // 重新初始化ADC以应用新的配置
    ADC_MODE = DC_MODE; // 更新当前模式为直流测量模式
    return HAL_OK;
}

HAL_StatusTypeDef ADC_SetACMode()
{

    ADC_ChannelConfTypeDef sConfig = {0};

    HAL_ADC_Stop_DMA(&hadc2); // 停止ADC DMA传输


    hadc2.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_2;
    hadc2.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_NONE;
    hadc2.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
    hadc2.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
    if (HAL_ADC_Init(&hadc2) != HAL_OK)
    {
        Error_Handler();
    }

    /** Configure Regular Channel*/
    sConfig.Channel = ADC_CHANNEL_VOPAMP2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_6CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;
    if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    ADC_MODE = AC_MODE; // 更新当前模式为交流测量模式
    return HAL_OK;
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC2 && ADC_MODE == AC_MODE)
    {
        HAL_ADC_Stop_DMA(&hadc2);
        //计算交流电压真有效值
        uint64_t sum = 0;
        sum = 0; // 重置sum以计算平方和

        for (int i = 0; i < ADC_BUFFER_LENGTH; i++) 
        {
            sum += (adc_value[i] - 4095) * (adc_value[i] - 4095); // 累加电压与平均值差值的平方
        }
        rms_voltage = sqrt(((double)sum) / ADC_BUFFER_LENGTH); // 计算真有效值
        // 这里可以将rms_voltage传递给OLED显示函数或者其他处理函数
        HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc_value, ADC_BUFFER_LENGTH); // 重新启动ADC DMA传输以继续测量
    }
    if(hadc->Instance == ADC2 && ADC_MODE == DC_MODE)
    {
        //DSP滤波处理
        arm_mean_q15((q15_t *)adc_value, ADC_BUFFER_DC_LENGTH, (q15_t *)&adc_after_filter);
        
    }
}
