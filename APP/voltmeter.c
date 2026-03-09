#include "voltmeter.h"
#include "main.h"
#include "stdio.h"
#include "my_ADC.h"
#include "power_buffer.h"
#include "oled.h"
#include "PGA.h"
#include "stm32g4xx.h"
#include "stm32g4xx_hal_gpio.h"

#define VOLT_OL_HOLD_MS 1000U
#define VOLT_OL_DOT_MS  220U

static uint8_t VoltMeter_IsOverRange(uint16_t adc_raw)
{
    static uint8_t extreme_active = 0U;
    static uint32_t extreme_start_tick = 0U;
    uint32_t now = HAL_GetTick();

    if ((adc_raw == 0U) || (adc_raw == 65535U)) {
        if (extreme_active == 0U) {
            extreme_active = 1U;
            extreme_start_tick = now;
            return 0U;
        }

        if ((now - extreme_start_tick) >= VOLT_OL_HOLD_MS) {
            return 1U;
        }
        return 0U;
    }

    extreme_active = 0U;
    extreme_start_tick = now;
    return 0U;
}

static void VoltMeter_ShowOverRange(const char *title)
{
    char disp_str[20];
    uint8_t dot_count = (uint8_t)((HAL_GetTick() / VOLT_OL_DOT_MS) % 4U);

    sprintf(disp_str, "%s", title);
    OLED_PrintString(0, 0, disp_str, &font16x16, OLED_COLOR_NORMAL);

    if (dot_count == 0U) {
        sprintf(disp_str, " 0L");
    } else if (dot_count == 1U) {
        sprintf(disp_str, " 0L.");
    } else if (dot_count == 2U) {
        sprintf(disp_str, " 0L..");
    } else {
        sprintf(disp_str, " 0L...");
    }
    OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
}


void VoltMeter_Init()
{
    HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_RESET); // 关闭ohm档连接
    PowerBuffer_Init(); // 初始化功率缓冲器
    PGA_Init(); // 初始化可编程增益放大器
}

void VoltMeter_DC20V_Start()
{
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);// 模拟地设置为中等电平
    PGA_ChangeGain(2); // 设置增益为2倍
    ADC_SetDCMode(); // 配置ADC为直流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_value, ADC_BUFFER_DC_LENGTH); // 启动ADC并使用DMA传输数据到adc_value数组
}

void VoltMeter_DC2000mV_Start()
{
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);// 模拟地设置为中等电平
    PGA_ChangeGain(16); // 设置增益为16倍
    ADC_SetDCMode(); // 配置ADC为直流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_value, ADC_BUFFER_DC_LENGTH); // 启动ADC并使用DMA传输数据到adc_value数组
}

void VoltMeter_AC_Start()
{
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);// 模拟地设置为中等电平
    PGA_ChangeGain(1); // 设置增益为1倍
    ADC_SetACMode(); // 配置ADC为交流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_value, ADC_BUFFER_LENGTH); // 启动ADC并使用DMA传输数据到adc_value数组
}

void VoltMeter_DC20V_Display()
{
    if (VoltMeter_IsOverRange(adc_value[0]) != 0U) {
        VoltMeter_ShowOverRange("DC 20V");
        return;
    }

    double voltage = DC_20V_Calc(adc_after_filter);
    char DispStr[20];
    sprintf(DispStr, "DC 20V");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, " %.3f V", voltage);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

void VoltMeter_DC2000mV_Display()
{
    if (VoltMeter_IsOverRange(adc_value[0]) != 0U) {
        VoltMeter_ShowOverRange("DC 2000mV");
        return;
    }

    double voltage = DC_2000mV_Calc(adc_after_filter)*1000; // 假设使用3.3V参考电压
    char DispStr[20];
    sprintf(DispStr, "DC 2000mV");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, " %.3f mV", voltage);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

void VoltMeter_AC_Display()
{
    if (VoltMeter_IsOverRange(adc_value[0]) != 0U) {
        VoltMeter_ShowOverRange("AC");
        return;
    }

    double voltage = ((((rms_voltage / 4096.0) * 1.65) * 1000 / 43));
    char DispStr[20];
    sprintf(DispStr, "AC");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, " %.3f V", voltage);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

