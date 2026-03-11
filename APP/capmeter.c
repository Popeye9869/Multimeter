#include "capmeter.h"

#include <stdio.h>

#include "PGA.h"
#include "adc.h"
#include "current.h"
#include "my_ADC.h"
#include "oled.h"
#include "opamp.h"
#include "power_buffer.h"
#include "stm32g4xx_hal_adc.h"
#include "stm32g4xx_hal_gpio.h"
#include "stm32g4xx_hal_tim.h"
#include "tim.h"

typedef struct {
	const char *name;
	double upper_f;
	uint32_t timeout_us;
	uint32_t refresh_ms;
} CapMeter_Range;

typedef struct {
	double capacitance_f;
	uint8_t valid;
	uint8_t over_range;
} CapMeter_Result;

typedef enum {
	CAP_STAGE_IDLE = 0,
	CAP_STAGE_DISCHARGE,
	CAP_STAGE_CHARGE_LOW,
	CAP_STAGE_CHARGE_HIGH,
} CapMeter_Stage;

static const CapMeter_Range g_cap_ranges[] = {
	{"20nF", 20e-9, 2000U, 120U},
	{"2uF", 2e-6, 50000U, 180U},
	{"200uF", 200e-6, 3000000U, 700U},
};

#define CAP_CURRENT_A                 500e-6
#define CAP_MEASURE_LOW_V             (0.2)
#define CAP_MEASURE_HIGH_V            (3.0)
#define CAP_DISCHARGE_TARGET_V        (-3.0)
#define CAP_SENSE_PGA_GAIN            2U
#define CAP_TIMER_HZ                  1000000U
#define CAP_FAST_ADC_POLL_MS          2U
#define CAP_FAST_ADC_SAMPLE_TIME      ADC_SAMPLETIME_6CYCLES_5
#define CAP_PATH_SETTLE_MS            2U
#define CAP_SWITCH_SETTLE_MS          1U
#define CAP_SENSE_SETTLE_MS           2U
#define CAP_INVALID_VOLTAGE_V         (-100.0)
#define CAP_DISCHARGE_PULSE_US        2000U
#define CAP_CHARGE_PULSE_US           1000U

static uint8_t g_cap_range_idx = 1U;
static uint8_t g_cap_auto_mode = 0U;
static const char *g_cap_manual_name = "2uF";
static CapMeter_Result g_last_result = {0.0, 0U, 0U};
static uint32_t g_last_measure_tick = 0U;
static CapMeter_Stage g_last_stage = CAP_STAGE_IDLE;
static double g_last_stage_voltage_v = 0.0;

static uint32_t CapMeter_GetTIM2ClockHz(void)
{
	uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

	if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1) {
		return pclk1;
	}

	return pclk1 * 2U;
}

static void CapMeter_StartTimerUs(void)
{
	uint32_t tim2_clk_hz = CapMeter_GetTIM2ClockHz();
	uint32_t prescaler = 0U;

	if (tim2_clk_hz > CAP_TIMER_HZ) {
		prescaler = (tim2_clk_hz / CAP_TIMER_HZ) - 1U;
	}

	HAL_TIM_Base_Stop(&htim2);
	__HAL_TIM_SET_PRESCALER(&htim2, prescaler);
	__HAL_TIM_SET_AUTORELOAD(&htim2, 0xFFFFFFFFU);
	__HAL_TIM_SET_COUNTER(&htim2, 0U);
	htim2.Instance->EGR = TIM_EGR_UG;
	HAL_TIM_Base_Start(&htim2);
}

static void CapMeter_ResetTimerUs(void)
{
	__HAL_TIM_SET_COUNTER(&htim2, 0U);
}

static uint32_t CapMeter_GetElapsedUs(void)
{
	return __HAL_TIM_GET_COUNTER(&htim2);
}

static void CapMeter_WaitUs(uint32_t duration_us)
{
	uint32_t start_us = CapMeter_GetElapsedUs();

	while ((CapMeter_GetElapsedUs() - start_us) < duration_us) {
	}
}

static HAL_StatusTypeDef CapMeter_SetupFastADC(void)
{
	ADC_ChannelConfTypeDef sConfig = {0};

	HAL_ADC_Stop_DMA(&hadc2);
	HAL_ADC_Stop(&hadc2);

	hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc2.Init.Resolution = ADC_RESOLUTION_12B;
	hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc2.Init.GainCompensation = 0;
	hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc2.Init.LowPowerAutoWait = DISABLE;
	hadc2.Init.ContinuousConvMode = ENABLE;
	hadc2.Init.NbrOfConversion = 1;
	hadc2.Init.DiscontinuousConvMode = DISABLE;
	hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
	hadc2.Init.DMAContinuousRequests = DISABLE;
	hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc2.Init.OversamplingMode = DISABLE;

	if (HAL_ADC_Init(&hadc2) != HAL_OK) {
		return HAL_ERROR;
	}

	sConfig.Channel = ADC_CHANNEL_VOPAMP2;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = CAP_FAST_ADC_SAMPLE_TIME;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK) {
		return HAL_ERROR;
	}

	if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK) {
		return HAL_ERROR;
	}

	if (HAL_ADC_Start(&hadc2) != HAL_OK) {
		return HAL_ERROR;
	}

	return HAL_OK;
}

static uint8_t CapMeter_ReadRaw16(uint16_t *raw16)
{
	uint32_t sample_raw;

	if (HAL_ADC_PollForConversion(&hadc2, CAP_FAST_ADC_POLL_MS) != HAL_OK) {
		return 0U;
	}

	sample_raw = HAL_ADC_GetValue(&hadc2);
	if (sample_raw > 4095U) {
		sample_raw = 4095U;
	}

	*raw16 = (uint16_t)(sample_raw << 4);
	return 1U;
}

static double CapMeter_ClampVoltage(double voltage_v)
{
	if (voltage_v < -3.3) {
		return -3.3;
	}

	if (voltage_v > 3.3) {
		return 3.3;
	}

	return voltage_v;
}

static void CapMeter_ConfigureSensePath(void)
{
	HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_RESET);
	PowerBuffer_SetLevel(POWER_BUFFER_LEVEL_MEDIUM);
	PGA_ChangeGain(CAP_SENSE_PGA_GAIN);
	HAL_Delay(CAP_SENSE_SETTLE_MS);
}

static void CapMeter_ConfigureDrivePath(Current_Type current)
{
	HAL_GPIO_WritePin(R_SW_GPIO_Port, R_SW_Pin, GPIO_PIN_SET);
	HAL_OPAMP_Start(&hopamp3);
	SetCurrent(current);
	HAL_Delay(CAP_SWITCH_SETTLE_MS);
}

static double CapMeter_ReadSenseVoltage(void)
{
	uint16_t raw16;
	double measured_v;

	if (CapMeter_ReadRaw16(&raw16) == 0U) {
		return CAP_INVALID_VOLTAGE_V;
	}

	measured_v = DC_20V_Calc(raw16);
	return CapMeter_ClampVoltage(measured_v);
}

static uint8_t CapMeter_WaitForVoltage(double threshold_v,
					   uint8_t wait_rising,
					   uint32_t timeout_us,
					   Current_Type drive_current,
					   uint32_t pulse_us,
					   double *last_voltage,
					   uint32_t *elapsed_drive_us)
{
	uint32_t accumulated_drive_us = 0U;

	for (;;) {
		double voltage_v;

		CapMeter_ConfigureSensePath();
		voltage_v = CapMeter_ReadSenseVoltage();

		if (voltage_v > (CAP_INVALID_VOLTAGE_V + 1.0)) {
			if (last_voltage != NULL) {
				*last_voltage = voltage_v;
			}
			g_last_stage_voltage_v = voltage_v;

			if ((wait_rising != 0U) && (voltage_v >= threshold_v)) {
				if (elapsed_drive_us != NULL) {
					*elapsed_drive_us = accumulated_drive_us;
				}
				return 1U;
			}

			if ((wait_rising == 0U) && (voltage_v <= threshold_v)) {
				if (elapsed_drive_us != NULL) {
					*elapsed_drive_us = accumulated_drive_us;
				}
				return 1U;
			}
		}

		if (accumulated_drive_us >= timeout_us) {
			return 0U;
		}

		if ((timeout_us - accumulated_drive_us) < pulse_us) {
			pulse_us = timeout_us - accumulated_drive_us;
		}

		if (drive_current == CURRENT_RANGE_N500uA) {
			g_last_stage = CAP_STAGE_DISCHARGE;
		} else if ((wait_rising != 0U) && (threshold_v <= CAP_MEASURE_LOW_V)) {
			g_last_stage = CAP_STAGE_CHARGE_LOW;
		} else {
			g_last_stage = CAP_STAGE_CHARGE_HIGH;
		}

		CapMeter_ConfigureDrivePath(drive_current);
		CapMeter_ResetTimerUs();
		CapMeter_WaitUs(pulse_us);
		accumulated_drive_us += pulse_us;
	}
}

static double CapMeter_CalcFromElapsedUs(uint32_t elapsed_us, double delta_v)
{
	if ((elapsed_us == 0U) || (delta_v <= 0.0)) {
		return 0.0;
	}

	return (CAP_CURRENT_A * ((double)elapsed_us / 1000000.0)) / delta_v;
}

static uint8_t CapMeter_SuggestRangeIndex(double capacitance_f)
{
	if (capacitance_f <= g_cap_ranges[0].upper_f) {
		return 0U;
	}

	if (capacitance_f <= g_cap_ranges[1].upper_f) {
		return 1U;
	}

	return 2U;
}

static void CapMeter_PrepareHardware(void)
{
	HAL_OPAMP_Start(&hopamp3);
	CapMeter_StartTimerUs();
	CapMeter_SetupFastADC();
	HAL_Delay(CAP_PATH_SETTLE_MS);
}

static CapMeter_Result CapMeter_MeasureByIndex(uint8_t idx)
{
	CapMeter_Result result = {0.0, 0U, 0U};
	double delta_v = CAP_MEASURE_HIGH_V - CAP_MEASURE_LOW_V;
	double charge_cap_f = 0.0;
	double last_voltage = 0.0;
	uint32_t charge_elapsed_us;
	uint32_t elapsed_to_low_us;
	uint32_t discharge_timeout_us;
	uint32_t timeout_us;

	if (idx >= (sizeof(g_cap_ranges) / sizeof(g_cap_ranges[0]))) {
		return result;
	}

	timeout_us = g_cap_ranges[idx].timeout_us;
	discharge_timeout_us = timeout_us * 2U;
	if (discharge_timeout_us < 20000U) {
		discharge_timeout_us = 20000U;
	}
	CapMeter_PrepareHardware();
	elapsed_to_low_us = 0U;
	charge_elapsed_us = 0U;

	g_last_stage = CAP_STAGE_DISCHARGE;
	if (CapMeter_WaitForVoltage(CAP_DISCHARGE_TARGET_V, 0U, discharge_timeout_us,
			CURRENT_RANGE_N500uA, CAP_DISCHARGE_PULSE_US, &last_voltage, NULL) == 0U) {
		result.over_range = 1U;
		return result;
	}

	g_last_stage = CAP_STAGE_CHARGE_LOW;
	if (CapMeter_WaitForVoltage(CAP_MEASURE_LOW_V, 1U, timeout_us,
			CURRENT_RANGE_500uA, CAP_CHARGE_PULSE_US, &last_voltage, &elapsed_to_low_us) == 0U) {
		result.over_range = 1U;
		return result;
	}

	g_last_stage = CAP_STAGE_CHARGE_HIGH;
	if (CapMeter_WaitForVoltage(CAP_MEASURE_HIGH_V, 1U, timeout_us,
			CURRENT_RANGE_500uA, CAP_CHARGE_PULSE_US, &last_voltage, &charge_elapsed_us) == 0U) {
		result.over_range = 1U;
		return result;
	}
	charge_cap_f = CapMeter_CalcFromElapsedUs(charge_elapsed_us, delta_v);
	if (charge_cap_f <= 0.0) {
		result.over_range = 1U;
		return result;
	}

	result.capacitance_f = charge_cap_f;
	result.valid = 1U;
	result.over_range = (result.capacitance_f > (g_cap_ranges[idx].upper_f * 1.10)) ? 1U : 0U;
	return result;
}

static CapMeter_Result CapMeter_RunMeasurement(void)
{
	CapMeter_Result result;
	uint8_t idx = g_cap_range_idx;

	if (g_cap_auto_mode == 0U) {
		return CapMeter_MeasureByIndex(idx);
	}

	for (;;) {
		result = CapMeter_MeasureByIndex(idx);
		if ((result.valid != 0U) && (result.over_range == 0U)) {
			g_cap_range_idx = CapMeter_SuggestRangeIndex(result.capacitance_f);
			return result;
		}

		if ((result.over_range == 0U) || (idx >= 2U)) {
			g_cap_range_idx = idx;
			return result;
		}

		idx++;
		g_cap_range_idx = idx;
	}
}

static uint8_t CapMeter_ShouldRefresh(void)
{
	uint32_t refresh_ms = g_cap_ranges[g_cap_range_idx].refresh_ms;

	if (g_last_result.valid == 0U) {
		return 1U;
	}

	if ((HAL_GetTick() - g_last_measure_tick) >= refresh_ms) {
		return 1U;
	}

	return 0U;
}

static void CapMeter_EnsureMeasurement(void)
{
	if (CapMeter_ShouldRefresh() == 0U) {
		return;
	}

	g_last_result = CapMeter_RunMeasurement();
	g_last_measure_tick = HAL_GetTick();
}

static void CapMeter_DisplayValue(const CapMeter_Result *result)
{
	char disp_str[24];

	if ((result == NULL) || (result->valid == 0U) || (result->over_range != 0U)) {
		snprintf(disp_str, sizeof(disp_str), " 0L");
		OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
		snprintf(disp_str, sizeof(disp_str), "S%d %+.2fV", (int)g_last_stage, g_last_stage_voltage_v);
		OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
		return;
	}

	if (result->capacitance_f >= 1e-6) {
		snprintf(disp_str, sizeof(disp_str), " %.3f uF", result->capacitance_f * 1e6);
	} else if (result->capacitance_f >= 1e-9) {
		snprintf(disp_str, sizeof(disp_str), " %.2f nF", result->capacitance_f * 1e9);
	} else {
		snprintf(disp_str, sizeof(disp_str), " %.1f pF", result->capacitance_f * 1e12);
	}
	OLED_PrintString(0, 20, disp_str, &font16x16, OLED_COLOR_NORMAL);
	snprintf(disp_str, sizeof(disp_str), "+/-500uA");
	OLED_PrintString(0, 40, disp_str, &font16x16, OLED_COLOR_NORMAL);
}

static void CapMeter_DisplayCommon(void)
{
	char disp_str[24];

	CapMeter_EnsureMeasurement();

	if (g_cap_auto_mode != 0U) {
		snprintf(disp_str, sizeof(disp_str), "CAP A %s", g_cap_ranges[g_cap_range_idx].name);
	} else {
		snprintf(disp_str, sizeof(disp_str), "CAP %s", g_cap_manual_name);
	}
	OLED_PrintString(0, 0, disp_str, &font16x16, OLED_COLOR_NORMAL);

	CapMeter_DisplayValue(&g_last_result);
}

void CapMeter_Init(void)
{
	g_cap_auto_mode = 0U;
	g_cap_range_idx = 1U;
	g_cap_manual_name = "2uF";
	g_last_result.valid = 0U;
	g_last_result.over_range = 0U;
	g_last_result.capacitance_f = 0.0;
	g_last_measure_tick = 0U;
	g_last_stage = CAP_STAGE_IDLE;
	g_last_stage_voltage_v = 0.0;

	CapMeter_PrepareHardware();
	SetCurrent(CURRENT_RANGE_N500uA);
}

void CapMeter_20nF_Start(void)
{
	g_cap_auto_mode = 0U;
	g_cap_range_idx = 0U;
	g_cap_manual_name = "20nF";
	g_last_result.valid = 0U;
	SetCurrent(CURRENT_RANGE_N500uA);
}

void CapMeter_2uF_Start(void)
{
	g_cap_auto_mode = 0U;
	g_cap_range_idx = 1U;
	g_cap_manual_name = "2uF";
	g_last_result.valid = 0U;
	SetCurrent(CURRENT_RANGE_N500uA);
}

void CapMeter_200uF_Start(void)
{
	g_cap_auto_mode = 0U;
	g_cap_range_idx = 2U;
	g_cap_manual_name = "200uF";
	g_last_result.valid = 0U;
	SetCurrent(CURRENT_RANGE_N500uA);
}

void CapMeter_Auto_Start(void)
{
	g_cap_auto_mode = 1U;
	g_cap_range_idx = 1U;
	g_last_result.valid = 0U;
	SetCurrent(CURRENT_RANGE_N500uA);
}

void CapMeter_20nF_Display(void)
{
	CapMeter_DisplayCommon();
}

void CapMeter_2uF_Display(void)
{
	CapMeter_DisplayCommon();
}

void CapMeter_200uF_Display(void)
{
	CapMeter_DisplayCommon();
}

void CapMeter_Auto_Display(void)
{
	g_cap_auto_mode = 1U;
	CapMeter_DisplayCommon();
}