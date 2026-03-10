#include "ohmmeter.h"
#include "PGA.h"
#include "main.h"
#include "opamp.h"
#include "current.h"
#include "oled.h"
#include "stdio.h"
#include "my_ADC.h"
#include "power_buffer.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_opamp.h"

typedef struct {
    Current_Type current;
    const char *name;
    double upper_ohm;
    uint32_t pga_gain;
    double adc_scale_ohm;
    double adc_offset_ohm;
} OhmMeter_Range;

typedef struct {
    double resistance_ohm;
    uint8_t valid;
} OhmMeter_Result;

static const OhmMeter_Range g_ohm_ranges[] = {
    /* 所有欧姆档位硬件参数和计算公式集中在这里，后续调校只需修改本表。 */
    {CURRENT_RANGE_500uA, "200R", 200.0, 64U, 2490.94202899, 47.2944847},
    {CURRENT_RANGE_500uA, "2k", 2000.0, 64U, 2490.94202899, 47.2944847},
    {CURRENT_RANGE_150uA, "20k", 20000.0, 16U, 33570.7020, 78.2},
    {CURRENT_RANGE_15uA, "200k", 200000.0, 16U, 330000.0, 60.0},
};

static uint8_t g_ohm_range_idx = 0U;
static uint8_t g_ohm_auto_mode = 0U;
static const char *g_manual_range_name = "200R";

#define OHM_DIODE_PGA_GAIN 16U
#define OHM_CONTINUITY_THRESHOLD_OHM 10.0


static double OhmMeter_CalcResistance(uint32_t adc_val, uint8_t range_idx)
{
    if (range_idx >= (sizeof(g_ohm_ranges) / sizeof(g_ohm_ranges[0]))) {
        return 0.0;
    }

    return (double)adc_val * g_ohm_ranges[range_idx].adc_scale_ohm / 65535.0
           - g_ohm_ranges[range_idx].adc_offset_ohm;
}

static void OhmMeter_ApplyRangeByIndex(uint8_t idx)
{
    if (idx >= (sizeof(g_ohm_ranges) / sizeof(g_ohm_ranges[0]))) {
        return;
    }

    g_ohm_range_idx = idx;
    SetCurrent(g_ohm_ranges[idx].current);
    PGA_ChangeGain(g_ohm_ranges[idx].pga_gain);
}

static OhmMeter_Result OhmMeter_Read(void)
{
    OhmMeter_Result result = {0.0, 0U};
    uint16_t raw = adc_after_filter;

    if ((raw == 0U) || (raw == 65535U)) {
        return result;
    }

    result.resistance_ohm = OhmMeter_CalcResistance(raw, g_ohm_range_idx);
    if (result.resistance_ohm < 0.0) {
        result.resistance_ohm = 0.0;
    }
    result.valid = 1U;

    return result;
}

static double OhmMeter_CalcDiodeVoltage(uint32_t adc_val)
{
    double voltage = DC_2000mV_Calc(adc_val)+2.436;

    if (voltage < 0.0) {
        return 0.0;
    }

    return voltage;
}

static uint8_t OhmMeter_ReadDiodeVoltage(double *voltage_v)
{
    uint16_t raw = adc_after_filter;

    if ((raw == 0U) || (raw == 65535U)) {
        return 0U;
    }

    *voltage_v = OhmMeter_CalcDiodeVoltage(raw);
    return 1U;
}

static uint8_t OhmMeter_SuggestRangeIndex(double resistance_ohm)
{
    if (resistance_ohm <= g_ohm_ranges[0].upper_ohm) {
        return 0U;
    }
    if (resistance_ohm <= g_ohm_ranges[1].upper_ohm) {
        return 1U;
    }
    if (resistance_ohm <= g_ohm_ranges[2].upper_ohm) {
        return 2U;
    }
    return 3U;
}

static uint8_t OhmMeter_AutoChooseRange(double resistance_ohm)
{
    uint8_t idx = g_ohm_range_idx;
    double lower = 0.0;
    double upper = g_ohm_ranges[idx].upper_ohm;

    if (idx > 0U) {
        lower = g_ohm_ranges[idx - 1U].upper_ohm * 0.85;
    }
    upper *= 1.10;

    if ((resistance_ohm >= lower) && (resistance_ohm <= upper)) {
        return idx;
    }

    return OhmMeter_SuggestRangeIndex(resistance_ohm);
}

static void OhmMeter_DisplayCommon(void)
{
    OhmMeter_Result result = OhmMeter_Read();
    char disp_str[24];

    if (g_ohm_auto_mode != 0U) {
        sprintf(disp_str, "OHM A %s", g_ohm_ranges[g_ohm_range_idx].name);
    } else {
        sprintf(disp_str, "OHM %s", g_manual_range_name);
    }
    OLED_PrintString(0, 0, disp_str, &font16x16, OLED_COLOR_NORMAL);

    if (result.valid == 0U) {
        sprintf(disp_str, " 0L");
        OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
        return;
    }

    if (result.resistance_ohm >= 1000.0) {
        sprintf(disp_str, " %.3f kOhm", result.resistance_ohm / 1000.0);
    } else {
        sprintf(disp_str, " %.2f Ohm", result.resistance_ohm);
    }
    OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

void OhmMeter_Init()
{

    HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_SET);//开启电流源输出
    HAL_OPAMP_Start(&hopamp3); // 启动运放作为电流源

    PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_LOW);

    OhmMeter_ApplyRangeByIndex(0U); // 默认200欧档，同时设置电流源和PGA增益

    ADC_SetDCMode(); // 配置ADC为直流测量模式
    HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);// 校准ADC

    g_ohm_auto_mode = 0U;
    g_ohm_range_idx = 0U;
    g_manual_range_name = "200R";
    
}

void OhmMeter_200_Ohm_Start()
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "200R";
    OhmMeter_ApplyRangeByIndex(0U);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_2k_Ohm_Start(void)
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "2k";
    OhmMeter_ApplyRangeByIndex(1U);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_20k_Ohm_Start(void)
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "20k";
    OhmMeter_ApplyRangeByIndex(2U);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_200k_Ohm_Start(void)
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "200k";
    OhmMeter_ApplyRangeByIndex(3U);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_Diode_Start(void)
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "DIODE";
    SetCurrent(CURRENT_RANGE_500uA);
    PGA_ChangeGain(OHM_DIODE_PGA_GAIN);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_Continuity_Start(void)
{
    g_ohm_auto_mode = 0U;
    g_manual_range_name = "CONT";
    OhmMeter_ApplyRangeByIndex(0U);
    ADC_StartDC_DMA(ADC_BUFFER_CONT_LENGTH);
}

void OhmMeter_Auto_Start(void)
{
    g_ohm_auto_mode = 1U;
    OhmMeter_ApplyRangeByIndex(1U);
    ADC_StartDC_DMA(ADC_BUFFER_DC_LENGTH);
}

void OhmMeter_200_Ohm_Display()
{
    OhmMeter_DisplayCommon();
}

void OhmMeter_2k_Ohm_Display(void)
{
    OhmMeter_DisplayCommon();
}

void OhmMeter_20k_Ohm_Display(void)
{
    OhmMeter_DisplayCommon();
}

void OhmMeter_200k_Ohm_Display(void)
{
    OhmMeter_DisplayCommon();
}

void OhmMeter_Diode_Display(void)
{
    double voltage_v;
    char disp_str[24];

    OLED_PrintString(0, 0, "DIODE 500uA", &font16x16, OLED_COLOR_NORMAL);

    if (OhmMeter_ReadDiodeVoltage(&voltage_v) == 0U) {
        sprintf(disp_str, " 0L");
        OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
        return;
    }

    sprintf(disp_str, " %.1f mV", voltage_v * 1000.0);
    OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

void OhmMeter_Continuity_Display(void)
{
    OhmMeter_Result result = OhmMeter_Read();
    char disp_str[24];

    OLED_PrintString(0, 0, "CONT 10R", &font16x16, OLED_COLOR_NORMAL);

    if (result.valid == 0U) {
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
        sprintf(disp_str, " 0L");
        OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
        return;
    }

    if (result.resistance_ohm < OHM_CONTINUITY_THRESHOLD_OHM) {
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET);
    }

    sprintf(disp_str, " %.2f Ohm", result.resistance_ohm);
    OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

void OhmMeter_Auto_Display(void)
{
    OhmMeter_Result result;
    uint8_t next_idx;

    g_ohm_auto_mode = 1U;
    result = OhmMeter_Read();

    if (result.valid != 0U) {
        next_idx = OhmMeter_AutoChooseRange(result.resistance_ohm);
        if (next_idx != g_ohm_range_idx) {
            OhmMeter_ApplyRangeByIndex(next_idx);
        }
    }

    OhmMeter_DisplayCommon();
}
