#include "freqmeter.h"
#include "app.h"
#include <stdio.h>

#include "comp.h"
#include "dac.h"
#include "main.h"
#include "stm32g4xx_hal_comp.h"
#include "stm32g4xx_hal_dac.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "tim.h"
#include "oled.h"
#include "power_buffer.h"
#include "my_ADC.h"

typedef struct {
	double frequency_hz;
	double duty_percent;
	uint8_t valid;
} FreqMeter_Result;

typedef struct {
	uint16_t psc;
	const char *name;
	double upper_hz;
} FreqMeter_Range;

static const FreqMeter_Range g_ranges[] = {
	{1699U, "20Hz", 20.0},
	{169U, "200Hz", 200.0},
	{16U, "2kHz", 2000.0},
	{1U, "20kHz", 20000.0},
	{0U, "200kHz", 200000.0},
};




static const char *g_freq_range_name = "20Hz";
static uint32_t g_tim1_clk_hz = 0U;
static uint8_t g_current_range_idx = 0U;
static uint8_t g_auto_mode = 0U;
static volatile uint32_t g_period_cnt_latched = 0U;
static volatile uint32_t g_high_cnt_latched = 0U;
static volatile uint32_t g_prescaler_div_latched = 1U;
static volatile uint8_t g_capture_valid = 0U;

static uint32_t FreqMeter_GetTIM1ClockHz(void)
{
	uint32_t pclk2 = HAL_RCC_GetPCLK2Freq();

	if ((RCC->CFGR & RCC_CFGR_PPRE2) == RCC_CFGR_PPRE2_DIV1) {
		return pclk2;
	}

	return pclk2 * 2U;
}

static void FreqMeter_StartWithPrescaler(uint16_t psc, const char *range_name)
{
	g_freq_range_name = range_name;
	g_capture_valid = 0U;

	HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_1);
	HAL_TIM_IC_Stop(&htim1, TIM_CHANNEL_2);

	__HAL_TIM_SET_PRESCALER(&htim1, psc);
	__HAL_TIM_SET_COUNTER(&htim1, 0U);
	htim1.Instance->EGR = TIM_EGR_UG;

	HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
	HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_2);
}

static void FreqMeter_ApplyRangeByIndex(uint8_t idx)
{
	if (idx >= (sizeof(g_ranges) / sizeof(g_ranges[0]))) {
		return;
	}

	g_current_range_idx = idx;
	FreqMeter_StartWithPrescaler(g_ranges[idx].psc, g_ranges[idx].name);
}

static uint8_t FreqMeter_SuggestRangeIndex(double freq_hz)
{
	if (freq_hz <= g_ranges[0].upper_hz) {
		return 0U;
	}
	if (freq_hz <= g_ranges[1].upper_hz) {
		return 1U;
	}
	if (freq_hz <= g_ranges[2].upper_hz) {
		return 2U;
	}
	if (freq_hz <= g_ranges[3].upper_hz) {
		return 3U;
	}
	return 4U;
}

static uint8_t FreqMeter_AutoChooseRange(double freq_hz)
{
	uint8_t idx = g_current_range_idx;
	double lower = 0.0;
	double upper = g_ranges[idx].upper_hz;

	if (idx > 0U) {
		lower = g_ranges[idx - 1U].upper_hz * 0.85;
	}
	upper *= 1.10;

	if ((freq_hz >= lower) && (freq_hz <= upper)) {
		return idx;
	}

	return FreqMeter_SuggestRangeIndex(freq_hz);
}

static FreqMeter_Result FreqMeter_Read(void)
{
	FreqMeter_Result result = {0.0, 0.0, 0U};
	uint32_t period_cnt;
	uint32_t high_cnt;
	uint32_t prescaler_div;
	uint8_t capture_valid;
	uint32_t primask;

	primask = __get_PRIMASK();
	__disable_irq();
	period_cnt = g_period_cnt_latched;
	high_cnt = g_high_cnt_latched;
	prescaler_div = g_prescaler_div_latched;
	capture_valid = g_capture_valid;
	if (primask == 0U) {
		__enable_irq();
	}

	if ((capture_valid == 0U) || (period_cnt == 0U) || (g_tim1_clk_hz == 0U)) {
		return result;
	}

	/* Cap high_cnt so duty never exceeds 100 %. */
	if (high_cnt > period_cnt) {
		high_cnt = period_cnt;
	}

	result.frequency_hz = (double)g_tim1_clk_hz / ((double)prescaler_div * (double)period_cnt);
	result.duty_percent = ((double)high_cnt * 100.0) / (double)period_cnt;
	result.valid = 1U;

	return result;
}

static void FreqMeter_DisplayCommon(void)
{
	FreqMeter_Result result = FreqMeter_Read();
	char disp_str[24];

	if (g_auto_mode != 0U) {
		snprintf(disp_str, sizeof(disp_str), "AUTO %s", g_freq_range_name);
	} else {
		snprintf(disp_str, sizeof(disp_str), "FREQ %s", g_freq_range_name);
	}
	OLED_PrintString(0, 0, disp_str, &font16x16, OLED_COLOR_NORMAL);

	if (result.valid == 0U) {
		snprintf(disp_str, sizeof(disp_str), "No Signal");
		OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);

		snprintf(disp_str, sizeof(disp_str), "D: --.-%%");
		OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
		return;
	}

	if (result.frequency_hz >= 1000.0) {
		snprintf(disp_str, sizeof(disp_str), "F:%7.2fk", result.frequency_hz / 1000.0);
	} else {
		snprintf(disp_str, sizeof(disp_str), "F:%7.2fHz", result.frequency_hz);
	}
	OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);

	snprintf(disp_str, sizeof(disp_str), "D:%7.2f%%", result.duty_percent);
	OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

void FreqMeter_Init(void)
{
	HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_RESET); // 关闭ohm档连接

	ADC_SetDCMode();

	PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_LOW);

	HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2047);
	HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
	

    HAL_COMP_Start(&hcomp1);

	HAL_TIM_Base_Start(&htim1);

	HAL_TIM_IC_Start_IT(&htim1, TIM_CHANNEL_1);
	HAL_TIM_IC_Start(&htim1, TIM_CHANNEL_2);

	g_tim1_clk_hz = FreqMeter_GetTIM1ClockHz();
	g_current_range_idx = 0U;
	g_auto_mode = 0U;
	g_capture_valid = 0U;
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if ((htim->Instance == TIM1) && (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)) {

		/* Always latch — let the reader handle validation. */
		g_period_cnt_latched    = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
		g_high_cnt_latched      = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
		g_prescaler_div_latched = htim->Instance->PSC + 1U;
		g_capture_valid         = 1U;

		if(g_mode!=APP_MODE_FREQ)
		{
			HAL_TIM_IC_Stop_IT(&htim1, TIM_CHANNEL_1);
			HAL_TIM_IC_Stop(&htim1, TIM_CHANNEL_2);
		}
	}
}

void FreqMeter_20Hz_Start(void)
{
	g_auto_mode = 0U;
	FreqMeter_ApplyRangeByIndex(0U);
}

void FreqMeter_200Hz_Start(void)
{
	g_auto_mode = 0U;
	FreqMeter_ApplyRangeByIndex(1U);
}

void FreqMeter_2kHz_Start(void)
{
	g_auto_mode = 0U;
	FreqMeter_ApplyRangeByIndex(2U);
}

void FreqMeter_20kHz_Start(void)
{
	g_auto_mode = 0U;
	FreqMeter_ApplyRangeByIndex(3U);
}

void FreqMeter_200kHz_Start(void)
{
	g_auto_mode = 0U;
	FreqMeter_ApplyRangeByIndex(4U);
}

void FreqMeter_Auto_Start(void)
{
	g_auto_mode = 1U;
	FreqMeter_ApplyRangeByIndex(2U);
}

void FreqMeter_20Hz_Display(void)
{
	FreqMeter_DisplayCommon();
}

void FreqMeter_200Hz_Display(void)
{
	FreqMeter_DisplayCommon();
}

void FreqMeter_2kHz_Display(void)
{
	FreqMeter_DisplayCommon();
}

void FreqMeter_20kHz_Display(void)
{
	FreqMeter_DisplayCommon();
}

void FreqMeter_200kHz_Display(void)
{
	FreqMeter_DisplayCommon();
}

void FreqMeter_Auto_Display(void)
{
	FreqMeter_Result result;
	uint8_t next_idx;

	g_auto_mode = 1U;
	result = FreqMeter_Read();

	if (result.valid != 0U) {
		next_idx = FreqMeter_AutoChooseRange(result.frequency_hz);
		if (next_idx != g_current_range_idx) {
			FreqMeter_ApplyRangeByIndex(next_idx);
		}
	}

	FreqMeter_DisplayCommon();
}
