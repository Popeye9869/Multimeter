#include "voltmeter.h"
#include "stdio.h"
#include "my_ADC.h"
#include "power_buffer.h"
#include "oled.h"
#include "PGA.h"


void VoltMeter_Init()
{
    PowerBuffer_Init(); // 初始化功率缓冲器
    PGA_Init(); // 初始化可编程增益放大器
}

void VoltMeter_DC20V_Start()
{
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);// 模拟地设置为中等电平
    PGA_ChangeGain(2); // 设置增益为2倍
    ADC_SetDCMode(); // 配置ADC为直流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_value, 1); // 启动ADC并使用DMA传输数据到adc_value数组
}

void VoltMeter_DC2000mV_Start()
{
    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);// 模拟地设置为中等电平
    PGA_ChangeGain(16); // 设置增益为16倍
    ADC_SetDCMode(); // 配置ADC为直流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC
    HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc_value, 1); // 启动ADC并使用DMA传输数据到adc_value数组
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
    double voltage = DC_20V_Calc(adc_value[0]);
    char DispStr[20];
    sprintf(DispStr, "DC 20V");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, "Voltage: %.3lf V", voltage);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

void VoltMeter_DC2000mV_Display()
{
    double voltage = DC_2000mV_Calc(adc_value[0]); // 假设使用3.3V参考电压
    char DispStr[20];
    sprintf(DispStr, "DC 2000mV");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, "Voltage: %.3lf mV", voltage*1000);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

void VoltMeter_AC_Display()
{

    double voltage = ((((rms_voltage / 4096.0) * 1.65) * 1000 / 43));
    char DispStr[20];
    sprintf(DispStr, "AC");
    OLED_PrintString(0, 0, DispStr, &font16x16, OLED_COLOR_NORMAL);
    sprintf(DispStr, "Voltage: %.2lf V", voltage);
    OLED_PrintString(0, 20, DispStr, &font16x16, OLED_COLOR_NORMAL);
}

