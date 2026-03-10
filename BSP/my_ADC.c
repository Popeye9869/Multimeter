#include "my_ADC.h"
#include "app.h"
#include "stm32g4xx_hal_adc.h"
#include <math.h>
#include <stdint.h>
#include "SEGGER_RTT.h"

#include "arm_math.h" // 包含CMSIS DSP库头文件

#define AC_MIN_VALID_SAMPLES 8U

static uint8_t ADC_IsZeroCrossing(uint16_t prev_sample, uint16_t curr_sample, uint16_t zero_level)
{
    int32_t prev_diff = (int32_t)prev_sample - (int32_t)zero_level;
    int32_t curr_diff = (int32_t)curr_sample - (int32_t)zero_level;

    if ((prev_diff == 0) || (curr_diff == 0))
    {
        return 1U;
    }

    return ((prev_diff < 0) && (curr_diff > 0)) || ((prev_diff > 0) && (curr_diff < 0));
}

static uint8_t ADC_FindZeroCrossingWindow(const uint16_t *samples, uint32_t length, uint16_t zero_level,
                                          uint32_t *start_index, uint32_t *end_index)
{
    uint32_t start = 0;
    uint32_t end = 0;
    uint8_t start_found = 0U;
    uint8_t end_found = 0U;

    for (uint32_t i = 1; i < length; ++i)
    {
        if (ADC_IsZeroCrossing(samples[i - 1], samples[i], zero_level) != 0U)
        {
            start = i;
            start_found = 1U;
            break;
        }
    }

    for (uint32_t i = length - 1; i > 0; --i)
    {
        if (ADC_IsZeroCrossing(samples[i - 1], samples[i], zero_level) != 0U)
        {
            end = i;
            end_found = 1U;
            break;
        }
    }

    if ((start_found == 0U) || (end_found == 0U) || (end <= start) || ((end - start + 1U) < AC_MIN_VALID_SAMPLES))
    {
        return 0U;
    }

    *start_index = start;
    *end_index = end;
    return 1U;
}

static uint8_t ADC_IsExtremum(const uint16_t *samples, uint32_t index)
{
    uint16_t prev = samples[index - 1U];
    uint16_t curr = samples[index];
    uint16_t next = samples[index + 1U];

    return (((curr >= prev) && (curr > next)) || ((curr <= prev) && (curr < next)));
}

static uint8_t ADC_FindExtremumWindow(const uint16_t *samples, uint32_t length,
                                      uint32_t *start_index, uint32_t *end_index)
{
    uint32_t start = 0;
    uint32_t end = 0;
    uint8_t start_found = 0U;
    uint8_t end_found = 0U;

    for (uint32_t i = 1; i < (length - 1U); ++i)
    {
        if (ADC_IsExtremum(samples, i) != 0U)
        {
            start = i;
            start_found = 1U;
            break;
        }
    }

    for (uint32_t i = length - 2U; i > 0; --i)
    {
        if (ADC_IsExtremum(samples, i) != 0U)
        {
            end = i;
            end_found = 1U;
            break;
        }
    }

    if ((start_found == 0U) || (end_found == 0U) || (end <= start) || ((end - start + 1U) < AC_MIN_VALID_SAMPLES))
    {
        return 0U;
    }

    *start_index = start;
    *end_index = end;
    return 1U;
}

static double ADC_CalcACRmsFromWindow(const uint16_t *samples, uint32_t start_index, uint32_t end_index, uint16_t zero_level)
{
    uint64_t sum = 0;
    uint32_t valid_length = end_index - start_index + 1U;

    for (uint32_t i = start_index; i <= end_index; ++i)
    {
        int32_t centered = (int32_t)samples[i] - (int32_t)zero_level;
        sum += (uint64_t)((int64_t)centered * (int64_t)centered);
    }

    return sqrt((double)sum / (double)valid_length);
}


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

HAL_StatusTypeDef ADC_StartDC_DMA(uint32_t sample_count)
{
    HAL_ADC_Stop_DMA(&hadc2);
    return HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc_value, sample_count);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC2 && ADC_MODE == AC_MODE)
    {
        uint32_t start_index = 0;
        uint32_t end_index = ADC_BUFFER_LENGTH - 1U;

        HAL_ADC_Stop_DMA(&hadc2);

        if (ADC_FindZeroCrossingWindow(adc_value, ADC_BUFFER_LENGTH, ADC_AC_ZERO_LEVEL, &start_index, &end_index) == 0U)
        {
            (void)ADC_FindExtremumWindow(adc_value, ADC_BUFFER_LENGTH, &start_index, &end_index);
        }

        rms_voltage = ADC_CalcACRmsFromWindow(adc_value, start_index, end_index, ADC_AC_ZERO_LEVEL);
        HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc_value, ADC_BUFFER_LENGTH); // 重新启动ADC DMA传输以继续测量
    }
    if(hadc->Instance == ADC2 && ADC_MODE == DC_MODE)
    {
        if (g_mode == APP_MODE_CONT)
        {
            arm_mean_q15((q15_t *)adc_value, ADC_BUFFER_CONT_LENGTH, (q15_t *)&adc_after_filter);
            return;
        }

        //DSP滤波处理
        arm_mean_q15((q15_t *)adc_value, ADC_BUFFER_DC_LENGTH, (q15_t *)&adc_after_filter);
        
    }
}
